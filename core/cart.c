#include "cart.h"

enum CART_REGION {
    ROM = 1,
    UNMAP = 2,
};

static uint8_t get_cart_region(uint16_t addr)
{
    return (((addr >= 0x8000 && addr <= 0xffff) << 0) |
            ((addr >= 0x4020 && addr <= 0x7fff) << 1));
}

void cart_rw(struct nes *nes, uint16_t addr, uint8_t *val, mem_mode_t mode)
{
    if (get_cart_region(addr) == ROM)
        mapper_rw(nes, addr, val, mode);
    else if ((get_cart_region(addr) == UNMAP) && (mode == READ))
        *val = 0xff;
}

// TODO: parse other informations from the header
int cart_parse_header(struct nes *nes, uint8_t *header)
{
    if (header[0] != 0x4e || header[1] != 0x45 ||
        header[2] != 0x53 || header[3] != 0x1a) {
        fprintf(stderr, "This file isn't NES cart file\n");
        return 1;
    } 
    nes->cart.info.prg_size = 16 * KB * header[4];
    nes->cart.info.chr_size = 8 * KB * header[5];
    nes->cart.info.prg_ram_size = 8 * KB * header[8];
    nes->cart.info.type = (header[7] >> 2) & 0x03;
    nes->cart.info.mapper = (header[7] & 0xf0) | (header[6] >> 4);
    nes->cart.info.mirroring = header[6] & 0x01;
    return 0;
}

void cart_print_info(struct rom_info *info)
{
    printf("--------cart info--------\n");
    printf("Type: %s\n", (info->type == 2) ? "NES2.0" : "iNES");
    printf("PRGcart size: %d\n", info->prg_size);
    printf("CHRcart size: %d\n", info->chr_size);
    printf("Mapper: %d\n", info->mapper);
    printf("Mirroring type: %s\n", (!info->mirroring) ? "HORIZONTAL" : "VERTICAL");
}

void cart_load(struct nes *nes, char *cart_path)
{
    struct cart *cart = &(nes->cart);
    FILE *fp = fopen(cart_path, "r");
    uint8_t header[16];

    if (!fp) {
        fprintf(stderr, "Can't open the cart file\n");
        exit(EXIT_FAILURE);
    } 
    fread(header, sizeof(uint8_t), 16, fp);
    cart_parse_header(nes, header);

    cart->prg_rom = malloc(sizeof(uint8_t) * cart->info.prg_size);
    if (!cart->prg_rom) {
        fprintf(stderr, "can't allocate PRGROM\n");
        goto malloc_error;
    }
    fread(cart->prg_rom, sizeof(uint8_t), cart->info.prg_size, fp);

    cart->chr_rom = malloc(sizeof(uint8_t) * cart->info.chr_size);
    if (!cart->chr_rom) {
        fprintf(stderr, "can't allocate CHRROM\n");
        goto malloc_error;
    }
    fread(cart->chr_rom, sizeof(uint8_t), cart->info.chr_size, fp);

    fclose(fp);
    return;

malloc_error:
    free(cart->prg_rom);
    free(cart->chr_rom);
    fclose(fp);
}

void cart_unload(struct nes *nes)
{
    free(nes->cart.chr_rom);
    free(nes->cart.prg_rom);
}
