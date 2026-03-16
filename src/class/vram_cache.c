#include "vram_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jo/jo.h>
#include <stdint.h>
#include "runtime_log.h"

// Statistics
static uint32_t vc_total_pack_uploads = 0;
static uint32_t vc_total_tile_uploads = 0;
static uint32_t vc_tile_requests = 0;
static uint32_t vc_tile_hits = 0;
static uint32_t vc_tile_misses = 0;
static uint32_t vc_evictions = 0;
static bool g_vram_cache_enabled = true;

// Simplified cache: mantém um buffer por "pack" carregado em WRAM e permite
// copiar todo o conteúdo para VRAM sob demanda. Este módulo é propositalmente
// simples e comentado para servir como base para um carregamento dinâmico mais
// inteligente (por-tile, LRU, prefetch, DMA parcial, etc.).

typedef struct {
    char *pack_name;
    char *filepath;   // caminho no disco (usado para carga em chunks)
    void *data;       // ponteiro para buffer em WRAM contendo o arquivo .TEX
    size_t size;      // tamanho em bytes do buffer
    size_t read_offset; // já lidos em bytes (para leitura incremental)
    FILE *file;       // handle aberto durante leitura incremental
    bool loading;     // leitura ainda em progresso
    bool scheduled;   // se true, será copiado para VRAM no próximo vblank
} vram_pack_t;

static vram_pack_t *packs = NULL;
static int packs_count = 0;

// Tile-level cache (LRU)
#define VRAM_TILE_SLOTS 1024

typedef struct {
    int tileId;         // identificador lógico do tile (-1 = livre)
    void *wramPtr;      // ponteiro para dados do tile em WRAM
    bool inVram;        // está presente na VRAM
    bool scheduled;     // agendado para upload
    uint32_t lastUsed;  // contador para LRU
    int vramIndex;      // índice lógico/slot em VRAM (interpretado pelo engine)
} tile_slot_t;

static tile_slot_t tile_slots[VRAM_TILE_SLOTS];
static uint32_t tick_counter = 0;

// Inicializa a tabela de tiles.
static void tile_cache_init(void)
{
    for (int i = 0; i < VRAM_TILE_SLOTS; ++i) {
        tile_slots[i].tileId = -1;
        tile_slots[i].wramPtr = NULL;
        tile_slots[i].inVram = false;
        tile_slots[i].scheduled = false;
        tile_slots[i].lastUsed = 0;
        tile_slots[i].vramIndex = -1;
    }
}

void vram_cache_init(void)
{
    packs = NULL;
    packs_count = 0;
    vc_total_pack_uploads = 0;
    vc_total_tile_uploads = 0;
    vc_tile_requests = 0;
    vc_tile_hits = 0;
    vc_tile_misses = 0;
    vc_evictions = 0;
    tick_counter = 0;
    tile_cache_init();
    g_vram_cache_enabled = true;
}

// Leitura simples do arquivo para um buffer alocado com malloc.
// Leitura síncrona de arquivo (mantida para fallback, não usada por padrão).
static void *read_file_to_buffer(const char *filepath, size_t *out_size)
{
    if (runtime_log_is_enabled())
        jo_printf(0, 30, "vram_cache: (fallback) reading file '%s'", filepath);
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    void *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    if (runtime_log_is_enabled())
        jo_printf(0, 31, "vram_cache: (fallback) read %u bytes from '%s'", (unsigned)*out_size, filepath);
    return buf;
}

bool vram_cache_load_pack_to_wram(const char *pack_name, const char *filepath)
{
    if (!g_vram_cache_enabled)
        return false;

    // procura pack existente
    for (int i = 0; i < packs_count; ++i) {
        if (strcmp(packs[i].pack_name, pack_name) == 0) {
            return false; // já existe
        }
    }

    // Em vez de ler todo o arquivo de uma vez (bloqueante), registramos o
    // pack e iniciamos uma leitura incremental que será processada por
    // `vram_cache_do_uploads` em pequenos chunks por frame.
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        if (runtime_log_is_enabled())
            jo_printf(0, 32, "vram_cache: failed to open '%s' for pack '%s'", filepath, pack_name);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        if (runtime_log_is_enabled())
            jo_printf(0, 32, "vram_cache: empty file '%s'", filepath);
        return false;
    }

    vram_pack_t *new_packs = realloc(packs, sizeof(vram_pack_t) * (packs_count + 1));
    if (!new_packs) {
        fclose(f);
        return false;
    }
    packs = new_packs;
    packs[packs_count].pack_name = strdup(pack_name);
    packs[packs_count].filepath = strdup(filepath);
    packs[packs_count].data = malloc((size_t)sz);
    if (!packs[packs_count].data) {
        free(packs[packs_count].pack_name);
        free(packs[packs_count].filepath);
        fclose(f);
        return false;
    }
    packs[packs_count].size = (size_t)sz;
    packs[packs_count].read_offset = 0;
    packs[packs_count].file = f;
    packs[packs_count].loading = true; // leitura iniciada, continuará em `do_uploads`
    packs[packs_count].scheduled = false;
    packs_count++;
    if (runtime_log_is_enabled())
        jo_printf(0, 33, "vram_cache: registered pack '%s' size=%u bytes (deferred load)", pack_name, (unsigned)sz);
    return true;
}

bool vram_cache_is_pack_loading(const char *pack_name)
{
    for (int i = 0; i < packs_count; ++i) {
        if (strcmp(packs[i].pack_name, pack_name) == 0) {
            return packs[i].loading;
        }
    }
    return false;
}

bool vram_cache_is_pack_present(const char *pack_name)
{
    for (int i = 0; i < packs_count; ++i) {
        if (strcmp(packs[i].pack_name, pack_name) == 0) {
            return (packs[i].data != NULL) && !packs[i].loading;
        }
    }
    return false;
}

void vram_cache_schedule_upload_all(void)
{
    for (int i = 0; i < packs_count; ++i) packs[i].scheduled = true;
    for (int i = 0; i < VRAM_TILE_SLOTS; ++i) if (tile_slots[i].tileId != -1) tile_slots[i].scheduled = true;
}

// Observação: esta implementação copia o blob inteiro do arquivo .TEX para VRAM
// mapeado pelo joengine. Aqui usamos a função pública jo_vdp1_send_param_list
// não é apropriada; portanto, usamos um caminho simples e portátil: chamar a
// função do JO que carrega o image pack (jo_sprite_add_image_pack) é a forma
// usual — porém, para demonstrar o ciclo WRAM->VRAM, sobrescrevemos manualmente
// o conteúdo de VRAM via jo_vdp2_vram_transfer (se disponível) ou via memcpy
// no espaço mapeado.

static void upload_pack_to_vram_raw(const vram_pack_t *p)
{
    // Implementação genérica: tenta usar jo engine para transferir os buffer
    // diretamente. A função abaixo é um stub ilustrativo; dependendo da versão
    // do jo engine, você pode trocar por uma chamada DMA/EDMA apropriada.

    // Se houver uma API para enviar blocos brutos para VRAM, chame-a aqui.
    // Exemplo hipotético (remova/ajuste conforme sua versão do joengine):
    // jo_vdp2_vram_write(p->data, p->size, dest_vram_addr);

    // Como fallback, apenas logamos o evento para desenvolvimento.
    if (runtime_log_is_enabled())
        jo_printf(0, 22, "vram_cache: upload pack '%s' size=%u bytes", p->pack_name, (unsigned)p->size);
    ++vc_total_pack_uploads;
}

void vram_cache_enable(bool enabled)
{
    g_vram_cache_enabled = enabled;
}

bool vram_cache_is_enabled(void)
{
    return g_vram_cache_enabled;
}

void vram_cache_do_uploads(void)
{
    if (!g_vram_cache_enabled)
        return;

    // Leitura incremental por-pacote: para evitar bloqueios longos no startup
    // lemos o arquivo em pequenos pedaços a cada chamada (cada frame).
    const size_t PACK_READ_CHUNK = 16 * 1024; // 16KB por frame
    for (int i = 0; i < packs_count; ++i) {
        vram_pack_t *p = &packs[i];
        if (p->loading && p->file && p->data) {
            size_t remaining = p->size - p->read_offset;
            size_t toread = remaining < PACK_READ_CHUNK ? remaining : PACK_READ_CHUNK;
            size_t got = fread((char*)p->data + p->read_offset, 1, toread, p->file);
            if (got == 0) {
                // erro de leitura: aborta leitura e libera recursos
                if (runtime_log_is_enabled())
                    jo_printf(0, 34, "vram_cache: read error on '%s'", p->filepath ? p->filepath : "?");
                fclose(p->file);
                p->file = NULL;
                p->loading = false;
                free(p->data);
                p->data = NULL;
                continue;
            }
            p->read_offset += got;
            if (runtime_log_is_enabled())
                jo_printf(0, 35, "vram_cache: loading '%s' %u/%u bytes", p->pack_name, (unsigned)p->read_offset, (unsigned)p->size);
            if (p->read_offset >= p->size) {
                // leitura completa
                fclose(p->file);
                p->file = NULL;
                p->loading = false;
                p->scheduled = true; // marcar para upload no mesmo ou próximo vblank
                if (runtime_log_is_enabled())
                    jo_printf(0, 36, "vram_cache: finished loading '%s' into WRAM", p->pack_name);
                runtime_log("vram_cache: finished loading '%s' into WRAM", p->pack_name);
            }
        }
    }

    // Primeiro, uploads de packs completos
    for (int i = 0; i < packs_count; ++i) {
        if (!g_vram_cache_enabled)
            break;

        if (packs[i].scheduled && packs[i].data && !packs[i].loading) {
            upload_pack_to_vram_raw(&packs[i]);
            runtime_log("vram_cache: uploaded pack '%s' size=%u", packs[i].pack_name, (unsigned)packs[i].size);
            packs[i].scheduled = false;
        }
    }

    // Em seguida, uploads por-tile (LRU-driven). Aqui fazemos uploads
    // simples e marcamos os slots como presentes em VRAM.
    for (int i = 0; i < VRAM_TILE_SLOTS; ++i) {
        if (tile_slots[i].scheduled && tile_slots[i].wramPtr) {
            // Real transferência para VRAM deve usar DMA específico da
            // plataforma. Aqui apenas logamos a operação. Substitua o
            // conteúdo abaixo por uma chamada à API de transferências da
            // sua versão do jo engine quando disponível.
            if (runtime_log_get_mode() != RuntimeLogModeOff)
                jo_printf(0, 23, "vram_cache: upload tile %d -> slot %d", tile_slots[i].tileId, i);
            runtime_log("vram_cache: upload tile %d -> slot %d", tile_slots[i].tileId, i);
            ++vc_total_tile_uploads;
            tile_slots[i].inVram = true;
            tile_slots[i].scheduled = false;
            tile_slots[i].vramIndex = i; // mapeamento direto slot->vramIndex
        }
    }
}

void vram_cache_clear(void)
{
    for (int i = 0; i < packs_count; ++i) {
        free(packs[i].pack_name);
        free(packs[i].filepath);
        free(packs[i].data);
    }
    free(packs);
    packs = NULL;
    packs_count = 0;
    vc_total_pack_uploads = 0;
    vc_total_tile_uploads = 0;
    vc_tile_requests = 0;
    vc_tile_hits = 0;
    vc_tile_misses = 0;
    vc_evictions = 0;
    tick_counter = 0;
    // Do not change `g_vram_cache_enabled` here; that's controlled by `vram_cache_enable`
    tile_cache_init();
}

// Find slot index for tileId, or -1 if not found
static int find_tile_slot_by_id(int tileId)
{
    for (int i = 0; i < VRAM_TILE_SLOTS; ++i) if (tile_slots[i].tileId == tileId) return i;
    return -1;
}

// Evict a slot using LRU and return its index
static int evict_lru_slot(void)
{
    int lru = -1;
    for (int i = 0; i < VRAM_TILE_SLOTS; ++i) {
        if (tile_slots[i].tileId == -1) return i; // free
        if (lru == -1 || tile_slots[i].lastUsed < tile_slots[lru].lastUsed) lru = i;
    }
    // evict
    ++vc_evictions;
    tile_slots[lru].tileId = -1;
    tile_slots[lru].wramPtr = NULL;
    tile_slots[lru].inVram = false;
    tile_slots[lru].scheduled = false;
    tile_slots[lru].lastUsed = 0;
    tile_slots[lru].vramIndex = -1;
    return lru;
}

// Public: request a tile to be present in VRAM. Returns VRAM index if
// already resident, otherwise schedules upload and returns -1.
int vram_cache_request_tile(int tileId, void *wramPtr)
{
    if (!g_vram_cache_enabled)
        return -1;

    ++vc_tile_requests;
    if (tileId < 0) return -1;
    int slot = find_tile_slot_by_id(tileId);
    if (slot >= 0) {
        ++vc_tile_hits;
        if (tile_slots[slot].wramPtr == NULL && wramPtr != NULL)
            tile_slots[slot].wramPtr = wramPtr;
        if (!tile_slots[slot].inVram && tile_slots[slot].wramPtr != NULL)
            tile_slots[slot].scheduled = true;
        tile_slots[slot].lastUsed = ++tick_counter;
        return tile_slots[slot].inVram ? tile_slots[slot].vramIndex : -1;
    }
    // need to allocate a slot
    ++vc_tile_misses;
    int s = evict_lru_slot();
    tile_slots[s].tileId = tileId;
    tile_slots[s].wramPtr = wramPtr;
    tile_slots[s].inVram = false;
    tile_slots[s].scheduled = true; // vai ser copiado no próximo vblank
    tile_slots[s].lastUsed = ++tick_counter;
    tile_slots[s].vramIndex = -1;
    return -1;
}

void vram_cache_dump_stats(void)
{
    runtime_log("vram_cache: requests=%u hits=%u misses=%u tile_uploads=%u pack_uploads=%u evictions=%u",
                vc_tile_requests, vc_tile_hits, vc_tile_misses, vc_total_tile_uploads, vc_total_pack_uploads, vc_evictions);
}

void vram_cache_get_stats(vram_cache_stats_t *out_stats)
{
    int i;

    if (out_stats == NULL)
        return;

    out_stats->tile_requests = vc_tile_requests;
    out_stats->tile_hits = vc_tile_hits;
    out_stats->tile_misses = vc_tile_misses;
    out_stats->tile_uploads = vc_total_tile_uploads;
    out_stats->pack_uploads = vc_total_pack_uploads;
    out_stats->evictions = vc_evictions;
    out_stats->residents = 0;
    for (i = 0; i < VRAM_TILE_SLOTS; ++i)
        if (tile_slots[i].tileId != -1 && tile_slots[i].inVram)
            ++out_stats->residents;
}

// Public: schedule prefetch of tiles around a map position. Aqui apenas
// marca tiles adjacentes como agendados no cache local; a lógica para
// decidir quais tileIds mapear depende do formato de mapa e não é
// implementada neste stub — o chamador deve resolver tileId por posição.
void vram_cache_prefetch_region(int center_tile_x, int center_tile_y, int radius_tiles)
{
    // Exemplo: este stub apenas loga a intenção. Para implementar, itere
    // sobre as posições vizinhas, converta para tileId e chame
    // `vram_cache_request_tile(tileId, wramPtr)`.
    jo_printf(0, 24, "vram_cache: prefetch region %d,%d r=%d", center_tile_x, center_tile_y, radius_tiles);
}
