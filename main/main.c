#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "font5x7.h"

static const char *TAG = "SH1106";

/* ================= I2C CONFIG ================= */

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_TIMEOUT_MS 1000

/* ================= OLED CONFIG ================= */

#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)

/* SH1106 specifics */
#define SH1106_COL_OFFSET 2

/* ================= FRAME BUFFER ================= */

static uint8_t buffer[OLED_WIDTH * OLED_PAGES];

static esp_err_t oled_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        OLED_ADDR,
        data,
        sizeof(data),
        I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t oled_write_data(const uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (!buf)
        return ESP_ERR_NO_MEM;

    buf[0] = 0x40; // data mode
    memcpy(buf + 1, data, len);

    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        OLED_ADDR,
        buf,
        len + 1,
        I2C_TIMEOUT_MS / portTICK_PERIOD_MS);

    free(buf);
    return ret;
}

/* ================= I2C INIT ================= */

static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(
        I2C_MASTER_NUM,
        conf.mode,
        0,
        0,
        0));
}

/* ================= SH1106 INIT ================= */

static void sh1106_init(void)
{
    oled_write_cmd(0xAE); // display OFF
    oled_write_cmd(0xD5);
    oled_write_cmd(0x80);
    oled_write_cmd(0xA8);
    oled_write_cmd(0x3F);
    oled_write_cmd(0xD3);
    oled_write_cmd(0x00);
    oled_write_cmd(0x40);
    oled_write_cmd(0xAD);
    oled_write_cmd(0x8B);
    oled_write_cmd(0xA1);
    oled_write_cmd(0xC8);
    oled_write_cmd(0xDA);
    oled_write_cmd(0x12);
    oled_write_cmd(0x81);
    oled_write_cmd(0x7F);
    oled_write_cmd(0xA4);
    oled_write_cmd(0xA6);
    oled_write_cmd(0xAF); // display ON
}

/* ================= BUFFER HELPERS ================= */

static void oled_clear(void)
{
    memset(buffer, 0x00, sizeof(buffer));
}

/* ===================Debug Code================= */
void print_bin(uint8_t n)
{
    for (int i = 7; i >= 0; i--)
        printf("%d", (n >> i) & 1);
}

void debug_draw_char_terminal(uint8_t x, uint8_t y, char c)
{
    // Let's use the letter 'e' as the example: {0x38, 0x54, 0x54, 0x54, 0x18}

    printf("\nVisualizing Character: '%c' at position (%d, %d)\n", c, x, y);
    printf("------------------------------------------------------------\n");
    if (c < ' ' || c > 'z')
    {
        c = ' ';
    }

    const uint8_t *glyph = font5x7[c - ' '];

    for (int i = 0; i < 5; i++)
    { // Loop through 5 columns
        uint8_t current_col = glyph[i];
        printf("\nCOLUMN %d (Hex: 0x%02X | Binary: ", i, current_col);
        print_bin(current_col);
        printf(")\n");

        for (int j = 0; j < 8; j++)
        { // Loop through 8 bits (rows)
            uint8_t mask = (1 << j);
            uint8_t is_pixel_on = current_col & mask;

            // Math for the buffer
            int byte_index = (y + j) / 8 * OLED_WIDTH + (x + i);
            int bit_index = (y + j) % 8;

            printf("\nbyte_index = (x=%d + y=%d) / 8 * SSD1306_WIDTH + (x=%d + i=%d): %d\n", x, y, x, i, byte_index);
            printf("bit_index = (y =%d + j=%d) modulo 8: %d\n", y, j, bit_index);
            printf("byte_index :%d, bit_index: %d\n", byte_index, bit_index);

            if (is_pixel_on)
            {
                buffer[byte_index] |= (1 << bit_index);
                printf("\n=====Value: buffer[byte_index] |= (1 << bit_index) ====== ");
                // print_bin(buffer[byte_index]);
                /*==================debug=====================*/
                printf("\n==================debug=====================\n");
                uint8_t before = buffer[byte_index];
                uint8_t mask = (uint8_t)(1 << bit_index);

                printf("byte[%d]\n", byte_index);

                printf("  before = ");
                print_bin(before);
                printf(" (0x%02X)\n", before);

                printf("  mask   = ");
                print_bin(mask);
                printf(" (1 << %d)\n", bit_index);

                buffer[byte_index] |= mask;

                printf("  after  = ");
                print_bin(buffer[byte_index]);
                printf(" (0x%02X)\n", buffer[byte_index]);
                printf("==================debug ends=====================");
                /*==============debug ends=====================*/
                // buffer[byte_index] |= (1 << bit_index);
                printf("\nPostion: (%d, %d)", x, y);
                printf("  [BIT %d: ON ] ", j);
                printf("Mask: ");
                print_bin(mask);
                printf(" -> Buffer[%d], flip bit %d\n", byte_index, bit_index);
            }
            else
            {
                printf("Postion: (%d, %d)", x, y);
                printf("  [BIT %d: OFF] ", j);
                printf("Mask: ");
                print_bin(mask);
                printf(" -> Buffer[%d],  off bit %d (Skip)\n", byte_index, bit_index);
                // printf(" -> (Skip)\n");
            }
        }
    }
}
/* ===============Debug Code Ends========================== */
/* ================= DRAW CHAR ================= */

static void oled_draw_char(uint8_t x, uint8_t y, char c)
{
    if (c < ' ' || c > 'z')
        c = ' ';

    const uint8_t *glyph = font5x7[c - ' '];

    for (int col = 0; col < 5; col++)
    {
        if (x + col >= OLED_WIDTH)
            break;

        uint8_t line = glyph[col];

        for (int row = 0; row < 8; row++)
        {
            if (y + row >= OLED_HEIGHT)
                break;

            if (line & (1 << row))
            {
                int byte_index =
                    ((y + row) / 8) * OLED_WIDTH + (x + col);
                int bit_index = (y + row) % 8;

                buffer[byte_index] |= (1 << bit_index);
            }
        }
    }
}

static void oled_draw_string(uint8_t x, uint8_t y, const char *s)
{
    while (*s)
    {
        // oled_draw_char(x, y, *s++);
        debug_draw_char_terminal(x, y, *s++);
        x += 6;
    }
}

/* ================= SH1106 DISPLAY UPDATE ================= */

static void sh1106_update(void)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {

        oled_write_cmd(0xB0 + page);              // page address
        oled_write_cmd(0x00 + SH1106_COL_OFFSET); // lower column
        oled_write_cmd(0x10);                     // higher column

        oled_write_data(
            &buffer[page * OLED_WIDTH],
            OLED_WIDTH);
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_LOGI(TAG, "Init I2C");
    i2c_master_init();

    ESP_LOGI(TAG, "Init SH1106 OLED");
    sh1106_init();

    oled_clear();
    oled_draw_string(0, 0, "Hello, World!");
    sh1106_update();

    ESP_LOGI(TAG, "Done");
}
