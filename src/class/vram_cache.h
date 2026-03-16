#ifndef VRAM_CACHE_H
#define VRAM_CACHE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t tile_requests;
	uint32_t tile_hits;
	uint32_t tile_misses;
	uint32_t tile_uploads;
	uint32_t pack_uploads;
	uint32_t evictions;
	uint32_t residents;
} vram_cache_stats_t;

// Inicializa o gerenciador de cache de VRAM/WRAM.
void vram_cache_init(void);

// Carrega um arquivo de tiles (ex: "cd/BLK/BLK.TEX") para Work RAM (WRAM).
// 'pack_name' é usado apenas como identificador lógico do conjunto de tiles.
// Retorna true em sucesso.
bool vram_cache_load_pack_to_wram(const char *pack_name, const char *filepath);

// Agendar upload de todos os tiles carregados para a VRAM. Pode ser chamado
// a partir do fluxo principal quando quiser que todos os tiles disponíveis
// sejam copiados para a VRAM (chamar preferencialmente durante VBlank).
void vram_cache_schedule_upload_all(void);

// Executa os uploads agendados para VRAM. Deve ser chamado dentro do VBlank
// ou imediatamente antes de desenhar o mapa para garantir que os tiles estejam
// prontos em VRAM.
void vram_cache_do_uploads(void);

// Força despejar (evict) o cache e liberar buffers WRAM.
void vram_cache_clear(void);

// Enable/disable vram cache activity (uploads & loads).
// Useful to test whether WRAM/VRAM communication is the bottleneck.
void vram_cache_enable(bool enabled);
bool vram_cache_is_enabled(void);

// Request a single tile to be present in VRAM. If the tile is already in
// VRAM this returns its VRAM index (>=0). If not present, the function
// schedules the tile for upload and returns -1; the caller can rely on
// `vram_cache_do_uploads` to perform pending uploads (typically in VBlank).
// `wramPtr` must point to the raw tile data in WRAM.
int vram_cache_request_tile(int tileId, void *wramPtr);

// Prefetch tiles near the given map coordinates (in tile units). This
// schedules neighboring tiles for upload with lower priority to reduce
// hitching when the camera moves.
void vram_cache_prefetch_region(int center_tile_x, int center_tile_y, int radius_tiles);

// Helpers to query pack loading state (useful for deferred loaders).
bool vram_cache_is_pack_loading(const char *pack_name);
bool vram_cache_is_pack_present(const char *pack_name);

// Dump runtime statistics for vram cache (uploads, hits, misses, evictions)
void vram_cache_dump_stats(void);
void vram_cache_get_stats(vram_cache_stats_t *out_stats);


#endif
