#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>
#include "FileStream.h"
#include "HashMap.h"

// Compatibility functions
bool Directory_Create(const char* folder) {
    #if WIN32
        return CreateDirectoryA(path, NULL);
    #else
        return mkdir(path, 0777) == 0;
    #endif
    return false;
}

// .WAVE format
struct wave_header_t {
    uint32_t     magic; // FE EC B7 E5
    uint32_t     version;
    uint32_t     size;
    float        sample_rate;
    uint32_t     sample_count;
    uint32_t     loop_start; // in samples
    uint32_t     loop_end; // usually the same as sample_count
    uint8_t      codec;
    uint8_t      channel_count;
    uint16_t     padding; // should be 00 00
    uint32_t     start_offset;
    uint32_t     interleave;
    uint32_t     extra_data_offset;
};

struct wave_t {
    uint32_t                 file_offset;
    wave_header_t            header;
    int                      adpcm_coeff[5][0x10];
    int                      adpcm_history1_16[5];
    int                      adpcm_history2_16[5];

    uint16_t*                samples;
};

// .ANIM format
struct anim_header_t {
    uint32_t     magic; // 0xA04F877A
    uint8_t      padding[3];
    uint8_t      version;
    uint32_t     header_size;
    uint16_t     anim_entry_count;
    uint16_t     frame_count;
    // 0x10
    uint32_t     animation_entry_list_offset;
    uint32_t     frame_list_offset;
    uint32_t     texture_entries_header_offset;
};

struct frame_data_t {
    uint16_t frame_id;
    int16_t  offset_y;
    int16_t  offset_x;
};
struct anim_entry_t {
    uint32_t      offset_to_string;
    uint32_t      unknowni_1;
    float         unknownf_1;
    uint32_t      frame_count;
    uint32_t      frame_data_offset;
    frame_data_t* frame_data;
};

struct vec_t {
    uint16_t v[2];
};
struct frame_piece_t {
    uint16_t       id;
    vec_t          dst[4];
    vec_t          src[4];
};
struct frame_t {
    uint32_t       magic; // 0x1B3C6AB1
    uint16_t       unk;
    uint16_t       piece_count;
    uint32_t       width;
    uint32_t       height;
    uint32_t       header_size;
    frame_piece_t* pieces;
};

struct texture_entries_header_t {
    uint32_t       magic; // 0xD3CE76CA
    uint32_t       unknown;
    uint32_t       texture_count;
    uint32_t       header_size;
    uint32_t       body_size;
    uint32_t       pixel_start_offset;
};
struct texture_entry_t {
    uint8_t        size_factor;
    uint8_t        unknown;
    uint8_t        pixel_count;
    uint8_t        padding;
    uint32_t       color_offset;
    uint32_t       alpha_offset;
};

struct anim_t {
    uint32_t                 file_offset;
    anim_header_t            header;
    texture_entries_header_t texture_entries_header;
    vector<anim_entry_t>     entries;
    vector<char*>            entryNames;
    vector<frame_t>          frames;
    vector<texture_entry_t>  textures;
    vector<SDL_Surface*>     frameSurfaces;
    vector<SDL_Surface*>     textureSurfaces;
};

// .IMAGE format
struct image_header_t {
    uint32_t magic; // 0x39B40E6A
    uint32_t unknown1;
    uint32_t unknown_count;
    uint32_t unknown2;
    uint32_t frame_offset;
    uint32_t texture_entries_header_offset;
};
struct image_t {
    uint32_t                 file_offset;
    image_header_t           header;
    frame_t                  frame;
    texture_entries_header_t texture_entries_header;
    vector<texture_entry_t>  textures;
    vector<SDL_Surface*>     textureSurfaces;
};

// .VOL format
struct vol_header_t {
    uint32_t magic; // 0xB53D32CB
    uint32_t unknown1;
    uint32_t unknown2;
    uint32_t header_size;

    uint32_t vol_file_size;
    uint32_t file_count;
    uint32_t file_list_offset;
};

#ifdef WIN32
#pragma pack(2)
struct vol_file_t {
    uint32_t string_offset;
    uint64_t vol_offset;
    uint32_t file_comp_size;
    uint32_t unknown_hash;
};
#else
struct vol_file_t {
    uint32_t string_offset;
    uint64_t vol_offset;
    uint32_t file_comp_size;
    uint32_t unknown_hash;
} __attribute__((__packed__));
#endif

struct vol_t {
    uint32_t              file_offset;
    vol_header_t          header;
    // uint32_t              unknown_hash_count;
    // vector<uint32_t>      unknown_hashes;
    vol_file_t*           files;
    vector<char*>         fileStrings;
    HashMap<vol_file_t*>* fileMap;
};

struct RSDK_AnimFrame {
    int X;
    int Y;
    int W;
    int H;
    int OffX;
    int OffY;
    int SheetNumber;
    int Duration;
    int ID;
};

struct RSDK_Animation {
    char* Name;
    int AnimationSpeed;
    int FrameToLoop;
    int Flags;
    vector<RSDK_AnimFrame> Frames;
};

uint8_t Font8x8_basic[128][8];

void         BlitText(SDL_Surface* dstSurf, const char* string, int x, int y, Uint32 color) {
    uint32_t* dstP = (uint32_t*)dstSurf->pixels;

    for (size_t i = 0; i < strlen(string); i++) {
		for (int by = 0; by < 8; by++) {
			for (int bx = 0; bx < 8; bx++) {
				if (Font8x8_basic[(int)string[i]][by] & 1 << bx) {
					dstP[(x + bx + i * 8) + (y + by) * dstSurf->w] = color;
				}
			}
		}
	}
}
void         BlitSurfaceTexture(SDL_Surface* dstSurf, SDL_Surface* srcSurf, SDL_Rect* dst, SDL_Rect* src, bool flip_x, bool flip_y) {
    // printf("x (src): %d\n", src->x);
    // printf("y (src): %d\n", src->y);
    // printf("w (src): %d\n", src->w);
    // printf("h (src): %d\n", src->h);
    //
    // printf("x (dst): %d\n", dst->x);
    // printf("y (dst): %d\n", dst->y);
    // printf("w (dst): %d\n", dst->w);
    // printf("h (dst): %d\n", dst->h);
    //
    // printf("flip_x: %s\n", flip_x ? "TRUE" : "FALSE");
    // printf("flip_y: %s\n", flip_y ? "TRUE" : "FALSE");
    //
    // printf("\n");

    bool rotate = src->w != dst->w;

    uint32_t* dstP = (uint32_t*)dstSurf->pixels;
    uint32_t* srcP = (uint32_t*)srcSurf->pixels;

    if (!rotate && !flip_x && !flip_y) {
        SDL_BlitSurface(srcSurf, src, dstSurf, dst);
    }
    else {
        int dx, dy, sx, sy;
        if (rotate) {
            for (int i = 0; i < dst->w * dst->h; i++) {
                dx = dst->x + (i % dst->w);
                dy = dst->y + (i / dst->w);
                sx = src->x + (i / dst->w);
                sy = src->y + (i % dst->w);

                dstP[dx + dy * dstSurf->w] = srcP[sx + sy * srcSurf->w];
            }
        }
        else {
            for (int i = 0; i < dst->w * dst->h; i++) {
                dx = dst->x + (i % dst->w);
                dy = dst->y + (i / dst->w);
                sx = src->x + (i % dst->w);
                sy = src->y + (i / dst->w);

                dstP[dx + dy * dstSurf->w] = srcP[sx + sy * srcSurf->w];
            }
        }
    }
}

SDL_Surface* GetPixelsFromTextureEntry(texture_entry_t entry, uint64_t file_offset, FileStream* reader) {
    SDL_Surface* result;

    int pixel_count = (entry.pixel_count << (entry.size_factor + 3));

    Uint16* pixels = (Uint16*)malloc(pixel_count * sizeof(Uint16));
    reader->Seek(file_offset + entry.color_offset);
    reader->ReadBytes(&pixels, pixel_count * sizeof(Uint16));

    Uint8* codes = (Uint8*)malloc(pixel_count * sizeof(Uint8));
    reader->Seek(file_offset + entry.alpha_offset);
    reader->ReadBytes(&codes, pixel_count * sizeof(Uint8));

    int textureWidth = 1 << entry.size_factor;
    int textureHeight = pixel_count / textureWidth;

    Uint32 depth = 16;
    Uint32 pitch = (depth >> 3) * textureWidth;
    Uint32 pixel_format = SDL_PIXELFORMAT_RGB565;

    Uint16* pixelSurface = (Uint16*)malloc(pitch * textureHeight * sizeof(Uint16));
    memset(pixelSurface, 0, pitch * textureHeight * sizeof(Uint16));

    bool running = true;
    int blockCount = 1 << (entry.size_factor - 3);
    int blockMask = blockCount - 1;

    Uint16* surfSrc = (Uint16*)pixels;
    Uint16* surfDst = (Uint16*)pixelSurface;
    for (int d = 0; running; d++) {
        int x_start = (d & blockMask) * 8;
        int y_start = (d / blockCount) * 8;
        int p_start = d * 64;

        for (int p = 0; p < 64; p++) {
            if (p_start + p >= pixel_count) { running = false; break; }

            int x = (p >> 0 & 1) + ((p >> 2 & 1) << 1) + ((p >> 4 & 1) << 2);
            int y = (p >> 1 & 1) + ((p >> 3 & 1) << 1) + ((p >> 5 & 1) << 2);
            surfDst[(x_start + x) + (y_start + y) * textureWidth] = surfSrc[p_start + p];
        }
    }

    // NOTE: Texture Unswizzling code taken from SDL2 PSP rendering
    // int j;
    // unsigned char *ydst = (unsigned char *)data;
    // for (blocky = 0; blocky < heightblocks; ++blocky) {
    //     unsigned char *xdst = ydst;
    //     for (blockx = 0; blockx < widthblocks; ++blockx) {
    //         unsigned int *block;
    //         block = (unsigned int*)xdst;
    //
    //         for (j = 0; j < 8; ++j) {
    //             *(block++) = *(src++);
    //             *(block++) = *(src++);
    //             *(block++) = *(src++);
    //             *(block++) = *(src++);
    //             block += dstpitch;
    //         }
    //         xdst += 16;
    //     }
    //     ydst += dstrow;
    // }

    // NOTE: This is Texture Detwiddling, which is also the real name of unswizzling
    // Size is texture size
    // void subdivide_and_move(int x1, int y1, int size, int op) {
    // 	if (size == 1) {
    // 		if (op == 0) {		/* Decode */
    // 			detwiddled[y1*imgsize+x1] = read_pixel();
    // 		} else {		/* Encode */
    // 			write_pixel(detwiddled[y1*imgsize+x1]);
    // 		}
    // 	} else {
    // 		int ns = size/2;
    // 		subdivide_and_move(x1, y1, ns, op);
    // 		subdivide_and_move(x1, y1+ns, ns, op);
    // 		subdivide_and_move(x1+ns, y1, ns, op);
    // 		subdivide_and_move(x1+ns, y1+ns, ns, op);
    // 	}
    // }

    SDL_Surface* image565 = SDL_CreateRGBSurfaceWithFormatFrom((void*)pixelSurface, textureWidth, textureHeight, depth, pitch, pixel_format);
    result = SDL_ConvertSurfaceFormat(image565, SDL_PIXELFORMAT_RGBA32, 0);

    // Apply alphas
    bool applyAlphas = true;
    bool allOrNothing = false;
    if (applyAlphas) {
        bool running = true;
        for (int d = 0; running; d++) {
            int x_start = (d & blockMask) * 8;
            int y_start = (d / blockCount) * 8;
            int p_start = d * 64;

            for (int p = 0; p < 64; p++) {
                if (p_start + p >= pixel_count) { running = false; break; }

                int x = (p >> 0 & 1) + ((p >> 2 & 1) << 1) + ((p >> 4 & 1) << 2);
                int y = (p >> 1 & 1) + ((p >> 3 & 1) << 1) + ((p >> 5 & 1) << 2);
                uint32_t* pixel = ((uint32_t*)result->pixels) + (x_start + x) + (y_start + y) * textureWidth;

                int pixelIndex = p_start + p;
                int alpha = (codes[pixelIndex >> 1] >> (((pixelIndex & 1) << 2)) ) & 0xF;
                    alpha = alpha | alpha << 4;
                if (allOrNothing && alpha < 0xFF)
                    alpha = 0x00;
                *pixel = (*pixel & 0x00FFFFFF) | (alpha << 24);
            }
        }
    }

    free(pixelSurface);
    free(pixels);
    free(codes);

    SDL_FreeSurface(image565);
    return result;
}

void         BlitSurfaceFromFrame(SDL_Surface* dstSurf, vector<SDL_Surface*>* textures, frame_t frame, int x, int y) {
    int flip_x = 0;
    int flip_y = 0;
    for (int i = 0; i < frame.piece_count; i++) {
        frame_piece_t p = frame.pieces[i];

        uint16_t min_src_x = p.src[0].v[flip_x] < p.src[1].v[flip_x] ? p.src[0].v[flip_x] : p.src[1].v[flip_x];
        uint16_t min_src_y = p.src[2].v[flip_y] < p.src[3].v[flip_x] ? p.src[2].v[flip_y] : p.src[3].v[flip_x];
        uint16_t max_src_x = p.src[0].v[flip_x] > p.src[1].v[flip_x] ? p.src[0].v[flip_x] : p.src[1].v[flip_x];
        uint16_t max_src_y = p.src[2].v[flip_y] > p.src[3].v[flip_x] ? p.src[2].v[flip_y] : p.src[3].v[flip_x];

        uint16_t min_dst_x = p.dst[0].v[flip_x] < p.dst[1].v[flip_x] ? p.dst[0].v[flip_x] : p.dst[1].v[flip_x];
        uint16_t min_dst_y = p.dst[2].v[flip_y] < p.dst[3].v[flip_x] ? p.dst[2].v[flip_y] : p.dst[3].v[flip_x];
        uint16_t max_dst_x = p.dst[0].v[flip_x] > p.dst[1].v[flip_x] ? p.dst[0].v[flip_x] : p.dst[1].v[flip_x];
        uint16_t max_dst_y = p.dst[2].v[flip_y] > p.dst[3].v[flip_x] ? p.dst[2].v[flip_y] : p.dst[3].v[flip_x];

        min_dst_y = frame.height - min_dst_y;
        max_dst_y = frame.height - max_dst_y;

        uint16_t swap = min_dst_y;
        min_dst_y = max_dst_y;
        max_dst_y = swap;

        SDL_Rect src = {
            min_src_x,  min_src_y,
            max_src_x - min_src_x,
            max_src_y - min_src_y
        };
        SDL_Rect dst = {
            x + min_dst_x, y + min_dst_y,
            max_dst_x - min_dst_x,
            max_dst_y - min_dst_y
        };

        // SDL_FillRect(dstSurf, &dst, 0xFFFF00FF);
        BlitSurfaceTexture(dstSurf, (*textures)[p.id], &dst, &src, p.dst[1].v[flip_x] == min_dst_x, p.dst[2].v[flip_y] == min_dst_y);
    }
}
SDL_Surface* GetSurfaceFromFrame(vector<SDL_Surface*>* textures, frame_t frame) {
    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, frame.width, frame.height, 32, SDL_PIXELFORMAT_RGBA32);

    BlitSurfaceFromFrame(result, textures, frame, 0, 0);

    return result;
}

void         ReadADPCMChannel(wave_t* wave, FileStream* reader, int cur_channel) {
    int sample_byte = 0;
    int first_sample = 0;
    int sample_count_total = wave->header.sample_count;

    int nibble_to_int[16] = { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };

    reader->Seek(wave->file_offset + wave->header.start_offset + wave->header.interleave * cur_channel);

    // reader->Seek(wave->file_offset + wave->header.start_offset + wave->header.interleave * cur_channel
    //     + framesin * 8);

    int sample_count = 0;
    for (;;) {
        int samples_to_do = 14;
        if (sample_count_total < samples_to_do)
            samples_to_do = sample_count_total;

        sample_count_total -= samples_to_do;
        if (sample_count_total == 0)
            break;

        Uint8 header = reader->ReadByte();

        int scale = 1 << (header & 0xF);
        int coef_index = (header >> 4) & 0xF;
        int hist1 = wave->adpcm_history1_16[cur_channel];
        int hist2 = wave->adpcm_history2_16[cur_channel];
        int coef1 = wave->adpcm_coeff[cur_channel][coef_index * 2];
        int coef2 = wave->adpcm_coeff[cur_channel][coef_index * 2 + 1];

        first_sample = first_sample % 14;

        for (int i = first_sample; i < first_sample + samples_to_do; i++, sample_count += wave->header.channel_count) {
            if ((i & 1) == 0)
                sample_byte = reader->ReadByte(); // sample_byte = read_8bit(framesin * 8 + stream->offset + 1 + i / 2, stream->streamfile);
            // reader->Seek(wave.file_offset + wave.header.start_offset + wave.header.interleave * cur_channel
            //     + framesin * 8 + 1 + i / 2);
            // sample_byte = reader->ReadByte();

            int sample = (((
                         ((i & 1) ? nibble_to_int[sample_byte & 0xF] : nibble_to_int[sample_byte >> 04])
                         * scale) << 11) + 1024 + (coef1 * hist1 + coef2 * hist2)) >> 11;

            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;

            wave->samples[sample_count + cur_channel] = (int16_t)sample;

            hist2 = hist1;
            hist1 = (int16_t)sample;
        }

        wave->adpcm_history1_16[cur_channel] = hist1;
        wave->adpcm_history2_16[cur_channel] = hist2;
    }
}

bool         printReadInfo = false;
const char*  weirdChamp = NULL;
// Sub Types
void         ReadFrame(frame_t* frame, uint64_t file_offset, FileStream* reader) {
    reader->Seek(file_offset);
    reader->ReadBytes(frame, sizeof(frame_t) - sizeof(frame_piece_t*));

    reader->Seek(file_offset + frame->header_size);

    if (printReadInfo) {
        printf("%-20s 0x%X\n", "Magic:", frame->magic);
        // printf("%-20s 0x%X\n", "Offset:", frame_offsets[i]);
        printf("%-20s %d\n", "Unknown:", frame->unk);
        printf("%-20s %d\n", "Rect Count:", frame->piece_count);
        printf("%-20s %d\n", "Width:", frame->width);
        printf("%-20s %d\n", "Height:", frame->height);
        printf("\n");
    }

    if (frame->piece_count != 0) {
        frame->pieces = (frame_piece_t*)malloc(frame->piece_count * sizeof(frame_piece_t));
        for (int d = 0; d < frame->piece_count; d++) {
            frame_piece_t frame_piece;
            reader->ReadBytes(&frame_piece, sizeof(frame_piece));
            frame->pieces[d] = frame_piece;

            if (printReadInfo)
                printf("Piece Info: Texture ID %-4d\n    DST (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n    SRC (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n",
                    frame_piece.id,
                    frame_piece.dst[0].v[0],
                    frame_piece.dst[0].v[1],
                    frame_piece.dst[1].v[0],
                    frame_piece.dst[1].v[1],
                    frame_piece.dst[2].v[0],
                    frame_piece.dst[2].v[1],
                    frame_piece.dst[3].v[0],
                    frame_piece.dst[3].v[1],
                    frame_piece.src[0].v[0],
                    frame_piece.src[0].v[1],
                    frame_piece.src[1].v[0],
                    frame_piece.src[1].v[1],
                    frame_piece.src[2].v[0],
                    frame_piece.src[2].v[1],
                    frame_piece.src[3].v[0],
                    frame_piece.src[3].v[1]);
        }
    }
    else {
        frame->piece_count = 1;
        frame->pieces = (frame_piece_t*)calloc(frame->piece_count, sizeof(frame_piece_t));
        for (int d = 0; d < frame->piece_count; d++) {
            frame_piece_t frame_piece;
            reader->ReadBytes(&frame_piece, sizeof(frame_piece));
            frame->pieces[d] = frame_piece;

            frame_piece.id = 0;
            // x1
            frame_piece.dst[0].v[0] = 0;
            frame_piece.dst[0].v[1] = 0;
            // x2
            frame_piece.dst[1].v[0] = frame->width;
            frame_piece.dst[1].v[1] = frame->width;
            // y1
            frame_piece.dst[2].v[0] = frame->height;
            frame_piece.dst[2].v[1] = frame->height;
            // y2
            frame_piece.dst[3].v[0] = 0;
            frame_piece.dst[3].v[1] = 0;

            frame_piece.src[0].v[0] = 0;
            frame_piece.src[0].v[1] = 0;

            frame_piece.src[1].v[0] = frame->width;
            frame_piece.src[1].v[1] = frame->width;

            frame_piece.src[2].v[0] = 0;
            frame_piece.src[2].v[1] = 0;

            frame_piece.src[3].v[0] = frame->height;
            frame_piece.src[3].v[1] = frame->height;

            if (printReadInfo)
                printf("Piece Info: Texture ID %-4d\n    DST (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n    SRC (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n",
                    frame_piece.id,
                    frame_piece.dst[0].v[0],
                    frame_piece.dst[0].v[1],
                    frame_piece.dst[1].v[0],
                    frame_piece.dst[1].v[1],
                    frame_piece.dst[2].v[0],
                    frame_piece.dst[2].v[1],
                    frame_piece.dst[3].v[0],
                    frame_piece.dst[3].v[1],
                    frame_piece.src[0].v[0],
                    frame_piece.src[0].v[1],
                    frame_piece.src[1].v[0],
                    frame_piece.src[1].v[1],
                    frame_piece.src[2].v[0],
                    frame_piece.src[2].v[1],
                    frame_piece.src[3].v[0],
                    frame_piece.src[3].v[1]);
        }
    }
    if (printReadInfo)
        printf("\n");
}
void         ReadTextureEntry(uint64_t file_offset, FileStream* reader) {

}

// Main Types
vol_t        ReadVOL(FileStream* reader) {
    vol_t vol;
    vol.fileMap = new HashMap<vol_file_t*>(NULL, 4);
    vol.file_offset = reader->Position();

    reader->ReadBytes(&vol.header, sizeof(vol.header));
    // vol.unknown_hash_count = reader->ReadUInt32();
    // for (int i = 0; i < vol.unknown_hash_count; i++) {
    //     vol.unknown_hashes.push_back(reader->ReadUInt32());
    // }

    if (printReadInfo) {
        printf("\n");
        printf("========================\n");
        printf(".VOL Header:\n");
        printf("========================\n\n");
        printf("%-32s 0x%X\n", "Magic:", vol.header.magic);
        printf("%-32s 0x%X\n", "Header Size:", vol.header.header_size);
        printf("%-32s 0x%X\n", "VOL File Size:", vol.header.vol_file_size);
        printf("%-32s %d\n", "File Count:", vol.header.file_count);
        printf("%-32s 0x%X\n", "File List Offset:", vol.header.file_list_offset);
        printf("\n");

        printf("========================\n");
        printf("File Entries\n");
        printf("========================\n\n");
    }

    reader->Seek(vol.file_offset + vol.header.file_list_offset);
    reader->ReadUInt32(); // Unknown hash?

    vol.files = (vol_file_t*)malloc(sizeof(vol_file_t) * vol.header.file_count);
    for (uint32_t i = 0; i < vol.header.file_count; i++) {
        reader->ReadBytes(&vol.files[i], 0x14);

        vol_file_t file = vol.files[i];

        char* str;
        uint64_t dis;

        dis = reader->Position();
        reader->Seek(vol.file_offset + file.string_offset);
        str = reader->ReadString();
        reader->Seek(dis);

        vol.fileStrings.push_back(str);

        vol.fileMap->Put(str, &vol.files[i]);

        if (printReadInfo) {
            printf("String Offset: %X (%s)\n", file.string_offset, str);
            printf("VOL Offset: %llX\n", file.vol_offset);
            printf("File Size: %X\n", file.file_comp_size);
            printf("Unknown Hash: %X\n", file.unknown_hash);
            printf("\n");
        }
    }
    return vol;
}
wave_t       ReadWAVE(FileStream* reader) {
    wave_t wave;

    wave.file_offset = reader->Position();

    reader->ReadBytes(&wave.header, sizeof(wave.header));
    // for (int i = 0; i < wave.header.version; i++) {
    //     reader->ReadUInt32();
    // }
    // animation_count = reader->ReadUInt32();

    // printReadInfo = true;
    if (printReadInfo) {
        printf("\n");
        printf("========================\n");
        printf(".WAVE Header:\n");
        printf("========================\n\n");
        printf("%-32s 0x%X\n", "Magic:", wave.header.magic);
        printf("%-32s 0x%X\n", "Version:", wave.header.version);
        printf("%-32s 0x%X\n", "WAVE File Size:", wave.header.size);
        printf("%-32s %.3f\n", "Sample Rate:", wave.header.sample_rate);
        printf("%-32s 0x%X\n", "Sample Count:", wave.header.sample_count);
        printf("%-32s 0x%X\n", "Loop Start:", wave.header.loop_start);
        printf("%-32s 0x%X\n", "Loop End:", wave.header.loop_end);
        printf("%-32s 0x%X\n", "Codec:", wave.header.codec);
        printf("%-32s 0x%X\n", "Channel Count:", wave.header.channel_count);
        printf("%-32s 0x%X\n", "Padding:", wave.header.padding);
        printf("%-32s 0x%X\n", "Start Offset:", wave.header.start_offset);
        printf("%-32s 0x%X\n", "Interleave Block Size:", wave.header.interleave);
        printf("%-32s 0x%X\n", "Extra Data Offset:", wave.header.extra_data_offset);
        printf("\n");

        printf("========================\n");
        printf("ADPCM Data\n");
        printf("========================\n\n");
    }

    reader->Seek(wave.file_offset + wave.header.extra_data_offset);
    for (int c = 0; c < wave.header.channel_count; c++) {
        for (int i = 0; i < 0x10; i++) {
            wave.adpcm_coeff[c][i] = reader->ReadInt16();
        }
        reader->ReadInt16(); // initial padding
        wave.adpcm_history1_16[c] = reader->ReadInt16();
        wave.adpcm_history2_16[c] = reader->ReadInt16();

        reader->ReadInt16(); // loop padding
        reader->ReadInt16(); // loop history1
        reader->ReadInt16(); // loop history2
    }

    if (printReadInfo) {
        for (int c = 0; c < wave.header.channel_count; c++) {
            printf("Channel %d:\n", c + 1);
            for (int i = 0; i < 0x10; i++) {
                printf("adpcm_coeff[%d]: %X\n", i, wave.adpcm_coeff[c][i]);
            }
            printf("adpcm_history1_16: %X\n", wave.adpcm_history1_16[c]);
            printf("adpcm_history2_16: %X\n", wave.adpcm_history2_16[c]);
            printf("\n");
        }
    }

    wave.samples = (uint16_t*)calloc(wave.header.sample_count, wave.header.channel_count * sizeof(uint16_t));

    for (int c = 0; c < wave.header.channel_count; c++) {
        ReadADPCMChannel(&wave, reader, c);
    }


    return wave;
}
anim_t       ReadANIM(FileStream* reader) {
    anim_t anim;

    anim.file_offset = reader->Position();

    uint32_t   animation_count;
    reader->ReadBytes(&anim.header, sizeof(anim.header));
    for (int i = 0; i < anim.header.version; i++) {
        reader->ReadUInt32();
    }
    animation_count = reader->ReadUInt32();

    if (printReadInfo) {
        printf("\n");
        printf("========================\n");
        printf(".ANIM Header:\n");
        printf("========================\n\n");
        printf("%-32s 0x%X\n", "Magic:", anim.header.magic);
        printf("%-32s 0x%X\n", "Version:", anim.header.version);
        printf("%-32s 0x%X\n", "RSDK_Animation Entry Count:", anim.header.anim_entry_count);
        printf("%-32s 0x%X\n", "RSDK_Animation Entry List Offset:", anim.header.animation_entry_list_offset);
        printf("%-32s %d\n", "Frame Count:", anim.header.frame_count);
        printf("%-32s 0x%X\n", "Frame List Offset:", anim.header.frame_list_offset);
        printf("%-32s 0x%X\n", "Pixel Data Offset:", anim.header.texture_entries_header_offset);
        printf("%-32s 0x%X\n", "Unknown Count:", animation_count);
        printf("\n");

        printf("========================\n");
        printf("RSDK_Animation Entries\n");
        printf("========================\n\n");
    }

    char* str;
    uint64_t dis;
    for (int i = 0; i < anim.header.anim_entry_count; i++) {
        anim_entry_t ae;
        reader->ReadBytes(&ae, 0x14);

        anim.entries.push_back(ae);
    }

    // Get the frame data
    for (int i = 0; i < anim.header.anim_entry_count; i++) {
        anim_entry_t* ae = &anim.entries[i];
        ae->frame_data = (frame_data_t*)calloc(ae->frame_count, sizeof(frame_data_t));

        dis = reader->Position();
        reader->Seek(anim.file_offset + ae->offset_to_string);
        str = reader->ReadString();
        reader->Seek(dis);

        anim.entryNames.push_back(str);

        reader->Seek(anim.file_offset + ae->frame_data_offset);

        if (printReadInfo) {
            printf("%-20s 0x%X (%s)\n", "String Offset:", ae->offset_to_string, str);
            printf("%-20s 0x%X\n", "Unknown Hash:", ae->unknowni_1);
            printf("%-20s %.3f\n", "Unknown Float:", ae->unknownf_1);
            printf("%-20s %d\n", "Frame Count:", ae->frame_count);
            printf("%-20s 0x%X\n", "Frame Data Offset:", ae->frame_data_offset);
        }
        for (uint32_t f = 0; f < ae->frame_count; f++) {
            reader->ReadBytes(&ae->frame_data[f], sizeof(frame_data_t));
            if (printReadInfo)
                printf("Info: ID %2d, X %4d, Y %4d\n",
                    ae->frame_data[f].frame_id,
                    ae->frame_data[f].offset_x,
                    ae->frame_data[f].offset_y);
        }
        if (printReadInfo)
            printf("\n");
    }

    if (printReadInfo) {
        printf("========================\n");
        printf("Frame Entries\n");
        printf("Count: %d\n", anim.header.frame_count);
        printf("========================\n\n");
    }

    vector<uint32_t> frame_offsets;
    reader->Seek(anim.file_offset + anim.header.frame_list_offset);
    for (int i = 0; i < anim.header.frame_count; i++) {
        frame_offsets.push_back(reader->ReadUInt32());
    }

    for (int i = 0; i < anim.header.frame_count; i++) {
        frame_t frame;
        ReadFrame(&frame, anim.file_offset + frame_offsets[i], reader);
        anim.frames.push_back(frame);
    }

    reader->Seek(anim.file_offset + anim.header.texture_entries_header_offset);

    reader->ReadBytes(&anim.texture_entries_header, sizeof(texture_entries_header_t));

    if (printReadInfo) {
        printf("========================\n");
        printf("Texture Entries\n");
        printf("Magic:              0x%X\n", anim.texture_entries_header.magic);
        printf("Texture Count:      %d\n", anim.texture_entries_header.texture_count);
        printf("Pixel Start Offset: 0x%X\n", anim.texture_entries_header.pixel_start_offset);
        printf("========================\n\n");
    }

    for (uint32_t i = 0; i < anim.texture_entries_header.texture_count; i++) {
        texture_entry_t entry;
        reader->ReadBytes(&entry, sizeof(entry));
        anim.textures.push_back(entry);

        if (printReadInfo) {
            printf("Size Factor: %2d, ", entry.size_factor);
            printf("Pixel Count: 0x%02X, ", entry.pixel_count);
            printf("Unknown: 0x%02X, ", entry.unknown);
            if (entry.padding) {
                printf("\n\nPadding was %02X!!!!!!!\n", entry.padding);
                exit(0);
            }
            printf("Color Data Offset: 0x%X, ", entry.color_offset);
            printf("Alpha Data Offset: 0x%X\n", entry.alpha_offset);
        }
    }

    for (uint32_t i = 0; i < anim.texture_entries_header.texture_count; i++) {
        anim.textureSurfaces.push_back(GetPixelsFromTextureEntry(anim.textures[i], anim.file_offset + anim.header.texture_entries_header_offset, reader));
    }

    return anim;
}
image_t      ReadIMAGE(FileStream* reader) {
    image_t image;
    image.file_offset = reader->Position();
    reader->ReadBytes(&image.header, sizeof(image.header));

    if (printReadInfo) {
        printf("\n");
        printf("========================\n");
        printf(".IMAGE Header:\n");
        printf("========================\n\n");
        printf("%-32s 0x%X\n", "Magic:", image.header.magic);
        printf("%-32s 0x%X (%d)\n", "Unknown 1:", image.header.unknown1, image.header.unknown1);
        printf("%-32s 0x%X (%d)\n", "Unknown Count:", image.header.unknown_count, image.header.unknown_count);
        printf("%-32s 0x%X (%d)\n", "Unknown 2:", image.header.unknown2, image.header.unknown2);
        printf("%-32s 0x%X\n", "Frame List Offset:", image.header.frame_offset);
        printf("%-32s 0x%X\n", "Texture Data Offset:", image.header.texture_entries_header_offset);
        printf("\n");

        printf("========================\n");
        printf("Frame Entries\n");
        // printf("Count: %d\n", image.header.unknown_count);
        printf("========================\n\n");
    }

    reader->Seek(image.file_offset + image.header.frame_offset);
    ReadFrame(&image.frame, image.file_offset + image.header.frame_offset, reader);

    reader->Seek(image.file_offset + image.header.texture_entries_header_offset);
    reader->ReadBytes(&image.texture_entries_header, sizeof(texture_entries_header_t));

    if (printReadInfo) {
        printf("========================\n");
        printf("Texture Entries\n");
        printf("Magic:              0x%X\n", image.texture_entries_header.magic);
        printf("Texture Count:      %d\n", image.texture_entries_header.texture_count);
        printf("Pixel Start Offset: 0x%X\n", image.texture_entries_header.pixel_start_offset);
        printf("========================\n\n");
    }

    for (uint32_t i = 0; i < image.texture_entries_header.texture_count; i++) {
        texture_entry_t entry;
        reader->ReadBytes(&entry, sizeof(entry));
        image.textures.push_back(entry);

        if (printReadInfo) {
            printf("Size Factor: %2d, ", entry.size_factor);
            printf("Pixel Count: 0x%02X, ", entry.pixel_count);
            printf("Unknown: 0x%02X, ", entry.unknown);
            if (entry.padding) {
                printf("\n\nPadding was %02X!!!!!!!\n", entry.padding);
                exit(0);
            }
            printf("Color Data Offset: 0x%X, ", entry.color_offset);
            printf("Alpha Data Offset: 0x%X\n", entry.alpha_offset);
        }
    }

    for (uint32_t i = 0; i < image.texture_entries_header.texture_count; i++) {
        image.textureSurfaces.push_back(GetPixelsFromTextureEntry(image.textures[i], image.file_offset + image.header.texture_entries_header_offset, reader));
    }

    return image;
}

// Extracting
void         ExtractIMAGE(image_t image, const char* filename, bool freeSurfs) {
    if (image.textureSurfaces.size() > 0) {
        SDL_Surface* result = GetSurfaceFromFrame(&image.textureSurfaces, image.frame);

        IMG_SavePNG(result, filename);
        SDL_FreeSurface(result);
    }

    if (freeSurfs) {
        for (size_t t = 0; t < image.textureSurfaces.size(); t++) {
            SDL_FreeSurface(image.textureSurfaces[t]);
        }
    }
}
void         ExtractANIM(anim_t anim, const char* filename, bool freeSurfs) {
    vector<RSDK_Animation> Animations;

    if (anim.textureSurfaces.size() > 0) {
        for (size_t t = 0; t < anim.frames.size(); t++) {
            anim.frameSurfaces.push_back(GetSurfaceFromFrame(&anim.textureSurfaces, anim.frames[t]));
        }

        size_t sheetWidth = 2;  // left/right padding
        size_t sheetHeight = 2; // top/bottom padding
        size_t anim_y = 1;
        for (size_t i = 0; i < anim.entries.size(); i++) {
            anim_y += 8 + 1;

            if (sheetWidth < 1 + strlen(anim.entryNames[i]) * 8 + 1)
                sheetWidth = 1 + strlen(anim.entryNames[i]) * 8 + 1;

            size_t startX = 1;
            size_t maxRowY = 0;
            for (Uint32 f = 0; f < anim.entries[i].frame_count; f++) {
                frame_data_t fd = anim.entries[i].frame_data[f];
                SDL_Surface* frame = anim.frameSurfaces[fd.frame_id];

                if (f > 0 && startX + frame->w > 1024) {
                    startX = 1;
                    anim_y += maxRowY + 1;
                    maxRowY = 0;
                }

                if (maxRowY < (size_t)frame->h)
                    maxRowY = (size_t)frame->h;

                if (sheetWidth < startX + frame->w + 1)
                    sheetWidth = startX + frame->w + 1;
                if (sheetHeight < anim_y + frame->h + 1)
                    sheetHeight = anim_y + frame->h + 1;

                startX += frame->w + 1;
            }
            anim_y += maxRowY + 1;
        }

        SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, sheetWidth, sheetHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_FillRect(result, NULL, SDL_MapRGB(result->format, 0x22, 0x22, 0x22));

        anim_y = 1;
        for (size_t i = 0; i < anim.entries.size(); i++) {
            BlitText(result, anim.entryNames[i], 1, anim_y, SDL_MapRGB(result->format, 0xF2, 0xD1, 0x41));


            RSDK_Animation an;
            an.Name = anim.entryNames[i];
            an.AnimationSpeed = 0x100;
            an.FrameToLoop = 0;
            an.Flags = 0;
            Animations.push_back(an);

            anim_y += 8 + 1;
            size_t startX = 1;
            size_t maxRowY = 0;
            for (Uint32 f = 0; f < anim.entries[i].frame_count; f++) {
                frame_data_t fd = anim.entries[i].frame_data[f];
                SDL_Surface* frame = anim.frameSurfaces[fd.frame_id];

                if (f > 0 && startX + frame->w > 1024) {
                    startX = 1;
                    anim_y += maxRowY + 1;
                    maxRowY = 0;
                }

                if (maxRowY < (size_t)frame->h)
                    maxRowY = frame->h;

                if (sheetWidth < startX + frame->w + 1)
                    sheetWidth = startX + frame->w + 1;
                if (sheetHeight < anim_y + frame->h + 1)
                    sheetHeight = anim_y + frame->h + 1;

                SDL_Rect dst = {
                    (int)startX, (int)anim_y,
                    frame->w, frame->h,
                };
                SDL_FillRect(result, &dst, SDL_MapRGBA(result->format, 0x00, 0x00, 0x00, 0x00));
                SDL_BlitSurface(frame, NULL, result, &dst);

                RSDK_AnimFrame anfrm;
                anfrm.SheetNumber = 0;
                anfrm.Duration = 0x100;
                anfrm.ID = 0;
                anfrm.X = startX;
                anfrm.Y = anim_y;
                anfrm.W = frame->w;
                anfrm.H = frame->h;
                anfrm.OffX = frame->w / -2;
                anfrm.OffY = frame->h / -2;
                Animations.back().Frames.push_back(anfrm);

                startX += frame->w + 1;
            }
            anim_y += maxRowY + 1;
        }

        IMG_SavePNG(result, filename);
        SDL_FreeSurface(result);

        for (size_t t = 0; t < anim.frames.size(); t++) {
            SDL_FreeSurface(anim.frameSurfaces[t]);
        }
    }

    if (freeSurfs) {
        for (size_t t = 0; t < anim.textureSurfaces.size(); t++) {
            SDL_FreeSurface(anim.textureSurfaces[t]);
        }
    }

    size_t str_len = strlen(filename);
    char animationFilename[512];
    strcpy(animationFilename, filename);
    animationFilename[str_len] = 0;
    animationFilename[str_len - 3] = 'b';
    animationFilename[str_len - 2] = 'i';
    animationFilename[str_len - 1] = 'n';

    FileStream* writer = FileStream::New(animationFilename, FileStream::WRITE_ACCESS);
    if (!writer) return;

    writer->WriteUInt32(0x00525053);
    writer->WriteUInt32(0x00000000);

    int sheets = 1;
    char suppie[256];
    writer->WriteByte(sheets);
    for (int i = 0; i < 1; i++) {
        sprintf(suppie, "%s%s", weirdChamp, strrchr(filename, '/'));
        writer->WriteHeaderedString(suppie);
    }

    writer->WriteByte(0);
    // int collisionboxes = reader->ReadByte();
    // for (int i = 0; i < collisionboxes; i++) {
    //     char* attr = reader->ReadRSDKString();
    //     // if (!strcmp(attr, "Solid"))
    //     //     solid = true;
    //     free(attr);
    // }

    writer->WriteUInt16(Animations.size()); // int count = reader->ReadUInt16();
    for (size_t a = 0; a < Animations.size(); a++) {
        RSDK_Animation an = Animations[a];
        writer->WriteHeaderedString(an.Name); // an.Name = reader->ReadRSDKString();
        writer->WriteUInt16(an.Frames.size()); // int frmCount = reader->ReadUInt16();
        writer->WriteUInt16(an.AnimationSpeed); // an.AnimationSpeed = reader->ReadUInt16();
        writer->WriteByte(an.FrameToLoop); // an.FrameToLoop = reader->ReadByte();
        writer->WriteByte(an.Flags); // an.Flags = reader->ReadByte(); // 0: Default behavior, 1: Full engine rotation, 2: Partial engine rotation, 3: Static rotation using extra frames, 4: Unknown (used alot in Mania)
        for (size_t i = 0; i < an.Frames.size(); i++) {
            RSDK_AnimFrame anfrm = an.Frames[i];
            writer->WriteByte(anfrm.SheetNumber); // anfrm.SheetNumber = reader->ReadByte();
            writer->WriteUInt16(anfrm.Duration); // anfrm.Duration = reader->ReadInt16();
            writer->WriteUInt16(0); // reader->ReadUInt16();
            writer->WriteUInt16(anfrm.X); // anfrm.X = reader->ReadUInt16();
            writer->WriteUInt16(anfrm.Y); // anfrm.Y = reader->ReadUInt16();
            writer->WriteUInt16(anfrm.W); // anfrm.W = reader->ReadUInt16();
            writer->WriteUInt16(anfrm.H); // anfrm.H = reader->ReadUInt16();
            writer->WriteInt16(anfrm.OffX); // anfrm.OffX = reader->ReadInt16(); // Center X
            writer->WriteInt16(anfrm.OffY); // anfrm.OffY = reader->ReadInt16(); // Center Y
        }
    }

    writer->Close();
}
void         ExtractWAVE(wave_t wave, const char* filename, bool freeData) {
    FileStream* writer = FileStream::New(filename, FileStream::WRITE_ACCESS);
    if (!writer) return;

    int bytesPerSample = 2;

    writer->WriteUInt32(0x46464952);
    writer->WriteUInt32(36 + wave.header.sample_count * wave.header.channel_count * bytesPerSample);
    writer->WriteUInt32(0x45564157);

    writer->WriteUInt32(0x20746D66);
    writer->WriteUInt32(16);
    writer->WriteUInt16(1);
    writer->WriteUInt16(wave.header.channel_count);
    writer->WriteUInt32((Uint32)wave.header.sample_rate);
    writer->WriteUInt32((Uint32)wave.header.sample_rate * wave.header.channel_count * bytesPerSample);
    writer->WriteUInt16(wave.header.channel_count * bytesPerSample);
    writer->WriteUInt16(bytesPerSample << 3);

    writer->WriteUInt32(0x61746164);
    writer->WriteUInt32(wave.header.sample_count * wave.header.channel_count * bytesPerSample);

    for (Uint32 i = 0; i < wave.header.sample_count * wave.header.channel_count; i++) {
        writer->WriteInt16(wave.samples[i]);
    }

    writer->Close();

    if (freeData) {
        // free samples
        free(wave.samples);
    }

    size_t str_len = strlen(filename);
    char animationFilename[512];
    strcpy(animationFilename, filename);
    animationFilename[str_len] = 0;
    animationFilename[str_len - 3] = 't';
    animationFilename[str_len - 2] = 'x';
    animationFilename[str_len - 1] = 't';

    writer = FileStream::New(animationFilename, FileStream::WRITE_ACCESS);
    if (!writer) return;

    char texttt[200];
    sprintf(texttt, "loop point: %d", wave.header.loop_start);

    writer->WriteBytes(texttt, strlen(texttt));
    writer->WriteByte(0);
    writer->Close();
}
void         ExtractVOL(const char* in_filename, const char* out_folder) {
    FileStream* reader = FileStream::New(in_filename, FileStream::READ_ACCESS);
    if (reader) {
        vol_t vol = ReadVOL(reader);

        Directory_Create(out_folder);

        // printReadInfo = true;

        char filename[256];
        for (size_t i = 0; i < vol.fileStrings.size(); i++) {
            printf("vol: %s\n", vol.fileStrings[i]);

            if (strstr(vol.fileStrings[i], ".wave")) {
                sprintf(filename, "%s%s%s.wav", out_folder, out_folder[strlen(out_folder) - 1] == '/' ? "" : "/", vol.fileStrings[i]);

                reader->Seek(vol.fileMap->Get(vol.fileStrings[i])->vol_offset);
                wave_t wave = ReadWAVE(reader);

                ExtractWAVE(wave, filename, true);
            }
            // /*
            else if (strstr(vol.fileStrings[i], ".image")) {
                sprintf(filename, "%s%s%s.png", out_folder, out_folder[strlen(out_folder) - 1] == '/' ? "" : "/", vol.fileStrings[i]);

                reader->Seek(vol.fileMap->Get(vol.fileStrings[i])->vol_offset);
                image_t image = ReadIMAGE(reader);

                ExtractIMAGE(image, filename, true);
            }
            else if (strstr(vol.fileStrings[i], ".anim")) {
                sprintf(filename, "%s%s%s.png", out_folder, out_folder[strlen(out_folder) - 1] == '/' ? "" : "/", vol.fileStrings[i]);

                reader->Seek(vol.fileMap->Get(vol.fileStrings[i])->vol_offset);
                anim_t anim = ReadANIM(reader);

                ExtractANIM(anim, filename, true);
            }
            //*/
            free(vol.fileStrings[i]);
        }
    }
}

int main(int argc, char* args[]) {
    FILE* res;
    if ((res = fopen("8x8_Font.bin", "r"))) {
        fread(Font8x8_basic, 1, 0x400, res);
    	fclose(res);
    }

    if (argc == 1) {
        printf("Usage:\n%s <vol-filename>\n", args[0]);
        return 0;
    }

    ExtractVOL(args[1], "output");
    return 0;
}
