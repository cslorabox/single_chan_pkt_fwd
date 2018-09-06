#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if defined(WIN32) || defined (__MINGW32__)
#include <windows.h>
typedef long suseconds_t;
#endif

#include "ll_ifc_transport_pc.h"
#include "ll_ifc.h"
#include "ll_ifc_no_mac.h"

#if defined(WIN32) || defined (__MINGW32__)
#define SLEEP(x)        Sleep(x * 1000)
#define MS_SLEEP(x)     Sleep(x)
#else
#define SLEEP(x)        sleep(x)
#define MS_SLEEP(x)     usleep(x * 1000)
#endif

#define DEFAULT_CHANNEL            (0)
#define DEFAULT_BANDWIDTH        (3)
#define SENTINEL                (255)

// Build Date
#define BUILD_DATE (__BUILD_DATE);

// Build Number
#define VERSION_MAJOR                (1) // 0-255
#define VERSION_MINOR                (4) // 0-255
#define VERSION_TAG     (__BUILD_NUMBER) // 0-65535

#define MAC_SET_TIMEOUT_SECS         (3)

typedef enum
{
    MODE_RX,
    MODE_RX_CONT,
    MODE_TX,
    MODE_SLEEP,
    MODE_ECHO,
    MODE_ECHO_TEST,
    MODE_ECHO_TX_PAYLOAD,
    MODE_NONE
} module_mode_t;

typedef struct _echo_test
{
    short cnt;
    char payload[64];
} echo_test_t;

static int g_print_help_exit = 0;

static void print_ll_ifc_error(char *label, int32_t ret_val)
{
    if (ret_val < 0)
    {
        fprintf(stderr, "ERROR(%s): Host interface - ", label);

        // Map Error code to NACK code
        if (ret_val >= -99 && ret_val <= -1)
        {
            ret_val = 0 - ret_val;
        }
        switch(ret_val)
        {
            case LL_IFC_NACK_CMD_NOT_SUPPORTED:
                fprintf(stderr, "NACK received - Command not supported");
                break;
            case LL_IFC_NACK_INCORRECT_CHKSUM:
                fprintf(stderr, "NACK received - Incorrect Checksum");
                break;
            case LL_IFC_NACK_PAYLOAD_LEN_OOR:
                fprintf(stderr, "NACK received - Payload length out of range");
                break;
            case LL_IFC_NACK_PAYLOAD_OOR:
                fprintf(stderr, "NACK received - Payload out of range");
                break;
            case LL_IFC_NACK_BOOTUP_IN_PROGRESS:
                fprintf(stderr, "NACK received - Not allowed, bootup in progress");
                break;
            case LL_IFC_NACK_BUSY_TRY_AGAIN:
                fprintf(stderr, "NACK received - Busy try again");
                break;
            case LL_IFC_NACK_APP_TOKEN_REG:
                fprintf(stderr, "NACK received - Application Token not registered");
                break;
            case LL_IFC_NACK_PAYLOAD_LEN_EXCEEDED:
                fprintf(stderr, "NACK received - Payload length greater than maximum");
                break;
            case LL_IFC_NACK_NOT_IN_MAILBOX_MODE:
                fprintf(stderr, "NACK received - Module is not in DOWNLINK_MAILBOX mode");
                break;
            case LL_IFC_NACK_NODATA:
                fprintf(stderr, "NACK received - No data available");
                break;
            case LL_IFC_NACK_OTHER:
                fprintf(stderr, "NACK received - Other");
                break;
            case LL_IFC_ERROR_INCORRECT_PARAMETER:
                fprintf(stderr, "Invalid Parameter");
                break;
            case -103:
                fprintf(stderr, "Message Number mismatch");
                break;
            case -104:
                fprintf(stderr, "Checksum mismatch");
                break;
            case -105:
                fprintf(stderr, "Command mismatch");
                break;
            case -106:
                fprintf(stderr, "Timed out");
                break;
            case -107:
                fprintf(stderr, "Payload larger than buffer provided");
                break;
            default:
                break;
        }
        fprintf(stderr, "\n");
    }
}

static void print_ll_version(void)
{
    ll_version_t ver;
    int8_t i8_ret;

    i8_ret = ll_version_get(&ver);
    print_ll_ifc_error("ll_version_get", i8_ret);
    if (i8_ret == VERSION_LEN)
    {
        printf("Link Labs Version: %d.%d.%d\n", ver.major, ver.minor, ver.tag);
    }
}

static void print_eui64(void)
{
    uint64_t uuid = 0;
    int8_t ret;

    ret = ll_unique_id_get(&uuid);
    print_ll_ifc_error("ll_unique_id_get", ret);
    if (ret >= 0)
    {
        printf("Link Labs EUI-64: %016llx\n", uuid);
    }
}

static void print_uuid(void)
{
    uint64_t uuid = 0;
    int8_t ret;

    ret = ll_unique_id_get(&uuid);
    uuid &= 0xFFFFFFFFF;
    print_ll_ifc_error("ll_unique_id_get", ret);
    if (ret >= 0)
    {
        printf("Link Labs UUID: $301$0-0-0-%01x", (uint32_t)(uuid >> 32));
        printf("%08x\n", (uint32_t)uuid);
    }
}

static void print_ll_firmware_type(void)
{
    ll_firmware_type_t t;
    int8_t ret;

    ret = ll_firmware_type_get(&t);
    print_ll_ifc_error("ll_firmware_type", ret);

    if (ret == FIRMWARE_TYPE_LEN)
    {
        printf("Link Labs Firmware Type: %04d.%04d\n", t.cpu_code, t.functionality_code);
    }
    else if (ret == -106) // timeout, assume module not present
    {
        exit(1);
    }
}

static void print_ll_hardware_type(void)
{
    ll_hardware_type_t t;
    int8_t ret;

    ret = ll_hardware_type_get(&t);
    print_ll_ifc_error("ll_hardware_type", ret);

    if (ret == 1)
    {
        switch(t)
        {
            case UNAVAILABLE:
                break;
            case LLRLP20_V2:
                printf("Link Labs Hardware Type: LLRLP20 v2\n");
                break;
            case LLRXR26_V2:
                printf("Link Labs Hardware Type: LLRXR26 v2\n");
                break;
            case LLRLP20_V3:
                printf("Link Labs Hardware Type: LLRLP20 v3\n");
                break;
            case LLRXR26_V3:
                printf("Link Labs Hardware Type: LLRXR26 v3\n");
                break;
            default:
                break;
        }
    }
}

#define RX_BUF_LEN          (255)

static void echo_test_mode(void)
{
    int8_t i32_ret;
    int c;
    int len;
    static echo_test_t test_buf;
    static uint8_t tx_buf[255];
    static uint8_t rx_buf[RX_BUF_LEN];
    uint8_t rx_len;

    test_buf.cnt = 0;
    while (1)
    {
        snprintf(test_buf.payload, sizeof(test_buf.payload), "Link-Labs!");
        len = sizeof(test_buf.cnt) + strlen(test_buf.payload);
        memcpy(tx_buf, &test_buf, len);

        i32_ret = ll_packet_send_queue(tx_buf, len);
        if (i32_ret != 0)
        {
            print_ll_ifc_error("ll_packet_send_queue", i32_ret);
        }
        else
        {
            printf("Tx(%3d): %s\n", test_buf.cnt, test_buf.payload);
        }
        test_buf.cnt++;

        i32_ret = ll_packet_recv_cont(rx_buf, RX_BUF_LEN, &rx_len, false);
        if(LL_IFC_NACK_NODATA != -i32_ret)
        {
            print_ll_ifc_error("ll_packet_recv_cont", i32_ret);
        }

        if (i32_ret == 0 && rx_len == 0)
        {
            SLEEP(5);
            i32_ret = ll_packet_recv_cont(rx_buf, RX_BUF_LEN, &rx_len, false);
            if(LL_IFC_NACK_NODATA != -i32_ret)
            {
                print_ll_ifc_error("ll_packet_recv_cont", i32_ret);
            }
        }

        if ((i32_ret == 0) && (rx_len > 0))
        {
            printf("Rx(%3d): ", (rx_buf[4] << 8) | rx_buf[3]);
            for (c = 5; c < rx_len - 2; c++)
            {
                printf("%c", rx_buf[c]);
            }
            printf(", rssi: %d, snr: %f", rx_buf[rx_len-2] + -137, (int8_t)rx_buf[rx_len-1] / 4.0);
            printf("\n");
        }
        else
        {
            printf("Rx(%3d): no packet\n", test_buf.cnt);
        }

        SLEEP(1);
    }
}

uint8_t rx_payload_is_hex = 0;

static void rx_mode_single(void)
{
    static uint8_t rx_buf[RX_BUF_LEN];
    uint8_t rx_len;
    int c;

    // Enter receive mode
    int8_t rv = ll_packet_recv(255, rx_buf, RX_BUF_LEN, &rx_len);
    print_ll_ifc_error("ll_packet_recv", rv);
    // Note intenionally ignoring return rx_buf here because
    // those are stale packets from the queue

    SLEEP(1);
    // Check module's queue for received packets
    rv = ll_packet_recv(255, rx_buf, RX_BUF_LEN, &rx_len);
    print_ll_ifc_error("ll_packet_recv", rv);

    if (rx_len > 0)
    {
        printf("Received %d byte packet:\n\t", rx_len);
        for (c = 0; c < rx_len; c++)
        {
            if (rx_payload_is_hex)
            {
                printf("0x%02x ", rx_buf[c]);
            }
            else
            {
                printf("%c", rx_buf[c]);
            }
        }
        printf("\n");
    }
    else
    {
        printf("No packets received\n");
    }
}

int32_t buffer_to_hex(char* buf)
{
    // only valid characters are 0:9,a:f,a:f
    uint16_t i;
    uint16_t num_chars = strlen(buf);
    if (num_chars&0x01)
    {
        fprintf(stderr, "hex payload must contain even number of characters\n");
        fprintf(stderr, "failed to send packet\n");
        exit(EXIT_FAILURE);
    }

    for(i=0; i<num_chars; i++)
    {
        char c = toupper(buf[i]);
        uint8_t x;

        // printf("\t\t%x\n", c);

        // 0:9
        if (isdigit(c))
        {
            x = c-'0';
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            x = c-'A'+10;
        }
        else
        {
            fprintf(stderr, "hex payload must only contain characters: 0-9, a-f, A-F\n");
            fprintf(stderr, "failed to send packet\n");
            exit(EXIT_FAILURE);
        }

        // pack the buffer
        if (i & 0x01)
        {
            // printf("low  [%d] = %x\n", (i-1)/2, x);
            buf[(i-1)/2] |= x;
        }
        else
        {
            // printf("high [%d] = %x\n", i/2, x<<4);
            buf[i/2] = x<<4;
        }
    }

    // printf("num_chars = %d\n", num_chars);
    buf[num_chars/2] = '\0';
    return strlen(buf);
}

static uint32_t timeval_diff_ms(struct timeval *begin, struct timeval *end)
{
    time_t tv_sec;
    suseconds_t tv_usec;

    tv_sec = end->tv_sec - begin->tv_sec;
    tv_usec = end->tv_usec - begin->tv_usec;
    return (tv_sec * 1000 + tv_usec / 1000);
}

static void rx_mode_cont(uint32_t receive_time_ms, uint8_t has_freq_err)
{
    static uint8_t rx_buf[RX_BUF_LEN];
    uint8_t rx_len;
    uint16_t c;
    uint16_t data_start;
    struct timeval tv_begin;

    gettimeofday(&tv_begin, NULL);
    while (1)
    {
        struct timeval tv_now;
        uint32_t num_ms;

        gettimeofday(&tv_now, NULL);
        num_ms = timeval_diff_ms(&tv_begin, &tv_now);
        if (receive_time_ms && num_ms > receive_time_ms)
        {
            break;
        }

        MS_SLEEP(50);
        int8_t rv = ll_packet_recv_cont(rx_buf, RX_BUF_LEN, &rx_len, (1 == has_freq_err));
        if(LL_IFC_NACK_NODATA != -rv)
        {
            print_ll_ifc_error("ll_packet_recv_cont", rv);
        }

        if (( rv == 0) && (rx_len > 0))
        {
            if (1 == has_freq_err)
            {
                printf("Received %d byte packet RSSI= %d SnR= %0.2f Freq Error=%d Hz:\n\t",
                    rx_len - 7, *((int16_t *)&rx_buf[0]), (int8_t)rx_buf[2] / 4.0, *(int32_t *)&rx_buf[3]);
                data_start = 7;
            }
            else
            {
                printf("Received %d byte packet RSSI= %d SnR= %0.2f:\n\t",
                    rx_len - 3, *((int16_t *)&rx_buf[0]), (int8_t)rx_buf[2] / 4.0);
                data_start = 3;
            }

            for (c = data_start; c < rx_len; c++)
            {
                if (rx_payload_is_hex)
                {
                    printf("0x%02x ", rx_buf[c]);
                }
                else
                {
                    printf("%c", rx_buf[c]);
                }
            }
            printf("\n");
        }
        else
        {
        }

        MS_SLEEP(20);
    }
}

static void print_irq_flags_text(uint32_t flags)
{
    // Most significant bit to least significant bit
    if(IRQ_FLAGS_ASSERT & flags)
    {
        printf("[IRQ_FLAGS_ASSERT]");
    }
    if(IRQ_FLAGS_APP_TOKEN_ERROR & flags)
    {
        printf("[IRQ_FLAGS_APP_TOKEN_ERROR]");
    }
    if(IRQ_FLAGS_CRYPTO_ERROR & flags)
    {
        printf("[IRQ_FLAGS_CRYPTO_ERROR]");
    }
    if(IRQ_FLAGS_DOWNLINK_REQUEST_ACK & flags)
    {
        printf("[IRQ_FLAGS_DOWNLINK_REQUEST_ACK]");
    }
    if(IRQ_FLAGS_INITIALIZATION_COMPLETE & flags)
    {
        printf("[IRQ_FLAGS_INITIALIZATION_COMPLETE]");
    }
    if(IRQ_FLAGS_APP_TOKEN_CONFIRMED & flags)
    {
        printf("[IRQ_FLAGS_APP_TOKEN_CONFIRMED]");
    }
    if(IRQ_FLAGS_CRYPTO_ESTABLISHED & flags)
    {
        printf("[IRQ_FLAGS_CRYPTO_ESTABLISHED]");
    }
    if(IRQ_FLAGS_DISCONNECTED & flags)
    {
        printf("[IRQ_FLAGS_DISCONNECTED]");
    }
    if(IRQ_FLAGS_CONNECTED & flags)
    {
        printf("[IRQ_FLAGS_CONNECTED]");
    }
    if(IRQ_FLAGS_RX_DONE & flags)
    {
        printf("[IRQ_FLAGS_RX_DONE]");
    }
    if(IRQ_FLAGS_TX_ERROR & flags)
    {
        printf("[IRQ_FLAGS_TX_ERROR]");
    }
    if(IRQ_FLAGS_TX_DONE & flags)
    {
        printf("[IRQ_FLAGS_TX_DONE]");
    }
    if(IRQ_FLAGS_RESET & flags)
    {
        printf("[IRQ_FLAGS_RESET]");
    }
    if(IRQ_FLAGS_WDOG_RESET & flags)
    {
        printf("[IRQ_FLAGS_WDOG_RESET]");
    }
    if(flags != 0)
    {
        printf("\n");
    }
}

static void echo_tx_with_payload(uint8_t* buf, uint16_t len)
{
    int32_t i32_ret;
    int c;
    static uint8_t rx_buf[RX_BUF_LEN];
    uint8_t rx_len;

    i32_ret = ll_packet_send_queue(buf, len);
    if ((i32_ret < 0) || (i32_ret > 1))
    {
        fprintf(stderr, "Failed to send packet: %d\n", i32_ret);
        exit(EXIT_FAILURE);
    }
    else if (i32_ret == 0) {
        printf("Message sent (send queue now full)\n");
    }
    else {
        printf("Message sent\n");
    }

    i32_ret = ll_packet_recv_cont(rx_buf, RX_BUF_LEN, &rx_len, false);
    if(LL_IFC_NACK_NODATA != -i32_ret)
    {
        print_ll_ifc_error("ll_packet_recv_cont", i32_ret);
    }
    if (i32_ret == 0 && rx_len == 0)
    {
        SLEEP(1);
        i32_ret = ll_packet_recv_cont(rx_buf, RX_BUF_LEN, &rx_len, false);
        if(LL_IFC_NACK_NODATA != -i32_ret)
        {
            print_ll_ifc_error("ll_packet_recv_cont", i32_ret);
        }
    }

    if ((i32_ret == 0) && (rx_len > 0))
    {
        printf("Rx: ");
        for (c = 3; c < rx_len - 2; c++)
        {
            printf("%c", rx_buf[c]);
        }
        printf(", rssi: %d, snr: %f", rx_buf[rx_len - 2] + -137, (int8_t)rx_buf[rx_len - 1] / 4.0);
        printf("\n");
    }
    else
    {
        printf("Rx: No packet\n");
    }
}

static void usage(void)
{
    printf("Usage:\n");
    printf("  --baudrate [-b] configure baudrate of the tty device, default: %d\n", LL_TTY_DEFAULT_BAUDRATE);
    printf("  --coding_rate [-C] configure coding rate [1-4] : 4/5, 4/6, 4/7, 4/8\n");
    printf("  --tone [-c] Transmit a CW tone\n");
    printf("  --delete_settings [-d] delete saved settings from flash\n");
    printf("  --device [-D] choose tty device, default: " LL_TTY_DEFAULT_DEVICE "\n");
    printf("  --echo_mode [-e] Enter echo slave mode\n");
    printf("  --echo_test [-E] Peer-to-peer test send/receive with matching echo mode end-node\n");
    printf("  --echo_tx_payload [-J] Single peer-to-peer send/receive with matching echo-mode slave node. Must specify payload.");
    printf("  --freq [-f] configure rx/tx frequency Hz\n");
    printf("  --get_radio_params [-g] Get the radio parameters of the module\n");
    printf("  --help [-h] print this help message\n");
    printf("  --tx_power_set [-p] configure tx output power\n");
    printf("          LLRLP20 [+2 to +20 dBm] LLRXR26 [+11 to +26 dBm]\n");
    printf("  --rx [-r] Place module in receive continuous mode\n");
    printf("  --rxh [-X] Place module in receive continuous mode, print out raw hex\n");
    printf("  --rxf [-Y] Place module in receive continuous mode, print out raw hex, with Rx Frequency error\n");
    printf("  --restore_defaults [-R] Restore default radio settings\n");
    printf("  --save_settings [-a] store radio settings to flash\n");
    printf("  --sleep [-s] Place module in sleep mode\n");
    printf("  --spreading_factor [-S] spreading factor [6-12]\n");
    printf("  --sync_word LoRa sync word [LoRaWAN: 0x34, default: 0x12]\n");
    printf("  --tx [-t] {tx message string} Place the module in transmit mode\n");
    printf("  --txh [-x] {tx hexadecimal data} Place the module in transmit mode\n");
    printf("  --bandwidth [-w] configure bandwidth [0-3] : 62.5k, 125k, 250k, 500k, default: %d\n", DEFAULT_BANDWIDTH);
    printf("  --reset_mcu [-u] Reset the module (takes a few seconds)\n");
    printf("  --bootloader [-U] Reset to Bootloader mode (takes a few seconds)\n");
    printf("  --key [-k] Set the encryption key\n");
    printf("  --mac_set MAC mode set\n");
    printf("  --mac_set_timeout number of seconds for mac_set timeout, default: %d\n", MAC_SET_TIMEOUT_SECS);
    printf("  --mac_get MAC mode get\n");
    printf("  --eui64 get the IEEE EUI-64 unique identifier\n");
    printf("  --uuid get unique identifier\n");
    printf("  --irq_flags_get get state of IRQ Flags Register\n");
    printf("  --irq_flags_clear clear bits in IRQ Flags Register\n");
    printf("  --preamble_len Set the preamble length in symbols\n");
    printf("  --sleep_block Block sleep \n");
    printf("  --sleep_unblock Unblock sleep \n");

    printf("  --antenna_set Set the antenna configuration \n");
    printf("  --antenna_get Get the antenna configuration \n");
    printf("  --receive_time Number of milliseconds to poll when in recieve mode, 0 indicates infinite loop, default: 0\n");
    printf("  --iq_inversion_set Set polarity: 0 for normal, 1 for inverted\n");

    printf("\n");
}

int main (int argc, char * const argv[])
{
    int c;
    int32_t i32_ret;    // Some function return int8_t's and some return int32_t's - know the difference!
    int8_t i8_ret;      // Some function return int8_t's and some return int32_t's - know the difference!

    /* Default options */
    int baudrate = LL_TTY_DEFAULT_BAUDRATE;
    uint8_t bandwidth = SENTINEL;
    uint8_t spreading_factor = SENTINEL;
    uint8_t coding_rate = SENTINEL;
    int8_t tx_power = -96;
    uint32_t frequency = 0;
    char *dev_name = NULL;
    module_mode_t mode = MODE_NONE;
    char *buf = NULL;
    uint8_t delete_settings = 0;
    uint8_t transmit_cw = 0;
    uint8_t store_settings = 0;
    uint8_t restore_defaults = 0;
    uint8_t get_radio_params = 0;
    int32_t preamble_len = -1;
    uint8_t block_sleep = 0;
    uint8_t unblock_sleep = 0;
    uint8_t reset_mcu = 0;
    uint8_t bootloader_mode = 0;
    //uint8_t setting_encryption_key = 0;
    uint8_t mac_set = SENTINEL;
    int32_t mac_set_timeout = MAC_SET_TIMEOUT_SECS;
    uint8_t mac_get = 0;
    uint8_t uuid = 0;
    uint8_t eui64 = 0;

    uint8_t tx_payload_is_hex = 0;

    uint8_t cmd_print_version = 0;
    uint8_t cmd_irq_flags_get = 0;
    uint8_t cmd_irq_flags_clear = 0;

    uint8_t cmd_antenna_get = 0;
    uint8_t cmd_antenna_set = 0;
    uint8_t antenna_cfg;

    uint8_t cmd_iq_inversion_set = 0;
    uint8_t iq_inversion_cfg;
    uint32_t receive_time_ms = 0;
    uint8_t cmd_sync_word = 0;
    uint8_t sync_word_cfg;

    uint8_t rx_cont_freq_err_requested = 0;

    /* Use getopt to parse command line arguments */
    while (1)
    {
        static struct option long_options[] =
        {
                {"version",             no_argument,       0, 'v'},
                {"baudrate",            required_argument, 0, 'b'},
                {"coding_rate",            required_argument, 0, 'C'},
                {"delete_settings",     no_argument,       0, 'd'},
                {"device",              required_argument, 0, 'D'},
                {"echo_mode",           no_argument,       0, 'e'},
                {"echo_test",           no_argument,       0, 'E'},
                {"echo_tx_payload",      required_argument, 0, 'J'},
                {"eui64",               no_argument,       0,   0},
                {"freq",                  required_argument, 0, 'f'},
                {"get_radio_params",    no_argument,       0, 'g'},
                {"irq_flags_clear",     required_argument, 0,   0},
                {"irq_flags_get",       no_argument,       0,   0},
                {"mac_set",             required_argument, 0,   0},
                {"mac_set_timeout",     required_argument, 0,   0},
                {"mac_get",             no_argument,       0,   0},
                {"uuid",                no_argument,       0,   0},
                {"reset_mcu",              no_argument,       0, 'u'},
                {"bootloader",          no_argument,       0, 'U'},
                {"tx_power_set",          required_argument, 0, 'p'},
                {"restore_defaults",    no_argument,       0, 'R'},
                {"rx",                    no_argument,        0, 'r'},
                {"save_settings",       no_argument,       0, 'a'},
                {"sleep",                no_argument,       0, 's'},
                {"spreading_factor",    required_argument, 0, 'S'},
                {"tx",                    required_argument, 0, 't'},
                {"txh",                 required_argument, 0, 'x'},
                {"tone",                no_argument,       0, 'c'},
                {"rxh",                 no_argument,       0, 'X'},
                {"rxf",                 no_argument,       0, 'Y'},
                {"bandwidth",            required_argument, 0, 'w'},
                {"key",                    required_argument, 0, 'k'},
                {"preamble_len",        required_argument, 0,   0},
                {"sleep_block",         no_argument,       0,   0},
                {"sleep_unblock",       no_argument,       0,   0},
                {"sync_word",           required_argument, 0,   0},
                {"antenna_set",         required_argument, 0,   0},
                {"antenna_get",         no_argument,       0,   0},
                {"iq_inversion_set",    required_argument, 0,   0},
                {"receive_time",        required_argument, 0,   0},
                {"help",                 no_argument,       0, 'h'},
                {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "ab:cC:dD:eEf:hJ:l:p:rRsS:t:uUvx:XYgw:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                if(strcmp(long_options[option_index].name, "mac_get") == 0)
                {
                    mac_get = 1;
                }
                else if(strcmp(long_options[option_index].name, "mac_set") == 0)
                {
                    mac_set = (uint8_t)strtol(optarg, NULL, 0);
                }
                else if(strcmp(long_options[option_index].name, "mac_set_timeout") == 0)
                {
                    mac_set_timeout = (int32_t)strtol(optarg, NULL, 0);
                }
                else if(strcmp(long_options[option_index].name, "antenna_set") == 0)
                {
                    antenna_cfg = (uint8_t)strtol(optarg, NULL, 0);
                    cmd_antenna_set = 1;
                }
                else if(strcmp(long_options[option_index].name, "antenna_get") == 0)
                {
                    cmd_antenna_get = 1;
                }
                else if(strcmp(long_options[option_index].name, "eui64") == 0)
                {
                    eui64 = 1;
                }
                else if(strcmp(long_options[option_index].name, "iq_inversion_set") == 0)
                {
                    iq_inversion_cfg = (uint8_t)strtol(optarg, NULL, 0);
                    cmd_iq_inversion_set = 1;
                }
                else if(strcmp(long_options[option_index].name, "receive_time") == 0)
                {
                    receive_time_ms = (uint32_t)strtol(optarg, NULL, 0);
                }
                else if(strcmp(long_options[option_index].name, "uuid") == 0)
                {
                    uuid = 1;
                }
                else if(strcmp(long_options[option_index].name, "irq_flags_get") == 0)
                {
                    cmd_irq_flags_get = 1;
                }
                else if(strcmp(long_options[option_index].name, "irq_flags_clear") == 0)
                {
                    buf = optarg;
                    cmd_irq_flags_clear = 1;
                }
                else if(strcmp(long_options[option_index].name, "get_radio_params") == 0)
                {
                    get_radio_params = 1;
                }
                else if(strcmp(long_options[option_index].name, "preamble_len") == 0)
                {
                    preamble_len = (int)(strtol(optarg, NULL, 0));
                }
                else if(strcmp(long_options[option_index].name, "sleep_block") == 0)
                {
                    block_sleep = 1;
                }
                else if(strcmp(long_options[option_index].name, "sleep_unblock") == 0)
                {
                    unblock_sleep = 1;
                }
                else if(strcmp(long_options[option_index].name, "sync_word") == 0)
                {
                    char *sync_word_buf = optarg;
                    // Convert from ASCII to integers
                    int32_t len = buffer_to_hex(sync_word_buf);

                    if (len == 1)
                    {
                        cmd_sync_word = 1;
                        sync_word_cfg = sync_word_buf[0];
                    }
                    else
                    {
                        fprintf(stderr, "Invalid sync word\n");
                    }
                }
                else
                {
                    printf("option %s", long_options[option_index].name);
                }
                break;

            case 'a':
                store_settings = 1;
                break;

            case 'b':
                baudrate = (int)strtol(optarg, NULL, 0);
                break;

            case 'C':
                coding_rate = (int)strtol(optarg, NULL, 0);
                break;

            case 'c':
                transmit_cw = 1;
                break;

            case 'd':
                delete_settings = 1;
                break;

            case 'D':
                dev_name = optarg;
                break;

            case 'e':
                mode = MODE_ECHO;
                break;

            case 'E':
                mode = MODE_ECHO_TEST;
                break;

            case 'f':
                frequency = (uint32_t)strtol(optarg, NULL, 0);
                break;

            case 'h':
                g_print_help_exit = 1;
                break;

            case 'J':
                mode = MODE_ECHO_TX_PAYLOAD;
                buf = optarg;
                tx_payload_is_hex = 0;
                rx_payload_is_hex = 0;
                break;

            case 'p':
                tx_power = (int)strtol(optarg, NULL, 0);
                break;

            case 'r':
                mode = MODE_RX_CONT;
                rx_payload_is_hex = 0;
                break;

            case 'X':
                mode = MODE_RX_CONT;
                rx_payload_is_hex = 1;
                rx_cont_freq_err_requested = 0;
                break;

            case 'Y':
                mode = MODE_RX_CONT;
                rx_payload_is_hex = 1;
                rx_cont_freq_err_requested = 1;
                break;

            case 'R':
                restore_defaults = 1;
                break;

            case 's':
                mode = MODE_SLEEP;
                break;

            case 'S':
                spreading_factor = (int)strtol(optarg, NULL, 0);
                break;

            case 't':
                mode = MODE_TX;
                buf = optarg;
                tx_payload_is_hex = 0;
                break;

            case 'x':
                mode = MODE_TX;
                buf = optarg;
                tx_payload_is_hex = 1;
                break;

            case 'g':
                get_radio_params = 1;
                break;

            case 'w':
                bandwidth = (int)strtol(optarg, NULL, 0);
                break;

            case 'u':
                reset_mcu = 1;
                break;

            case 'v':
                cmd_print_version = 1;
                break;

            case 'U':
                bootloader_mode = 1;
                break;

            case 'k':
                //setting_encryption_key = 1;
                buf = optarg;

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                abort();
        }
    }
    if (g_print_help_exit)
    {
        usage();
        exit(EXIT_SUCCESS);
    }
    if(cmd_print_version)
    {
        printf("Executable Version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_TAG);
    }

    ll_tty_open(dev_name, baudrate);

    if(SENTINEL != mac_set)
    {
        ll_mac_type_t curr_mac_mode;
        int32_t cnt;

        cnt = 0;
        do
        {
            i32_ret = ll_mac_mode_set(mac_set);
            cnt += 2;
        } while (i32_ret < 0 && cnt < mac_set_timeout);
        print_ll_ifc_error("ll_mac_mode_set", i32_ret);
        if (i32_ret < 0)
        {
            printf("set MAC mode to undefined\n");
            exit(EXIT_FAILURE);
        }

        MS_SLEEP(400);

        do
        {
            cnt += 1;
            i32_ret = ll_mac_mode_get(&curr_mac_mode);
            if (curr_mac_mode == (ll_mac_type_t) mac_set)
            {
                break;
            }
        } while (i32_ret < 0 && cnt < mac_set_timeout);
        if (i32_ret >= 0)
        {
            printf("set MAC mode to %d\n", mac_set);
        }
    }

    print_ll_firmware_type();
    print_ll_hardware_type();
    print_ll_version();
    print_uuid();

    if (frequency != 0)
    {
        i32_ret = ll_frequency_set(frequency);
        print_ll_ifc_error("ll_frequency_set", i32_ret);
    }
    if (bandwidth != SENTINEL)
    {
        i32_ret = ll_bandwidth_set(bandwidth);
        print_ll_ifc_error("ll_bandwidth_set", i32_ret);
    }
    if (spreading_factor != SENTINEL)
    {
        i32_ret = ll_spreading_factor_set(spreading_factor);
        print_ll_ifc_error("ll_spreading_factor_set", i32_ret);
    }
    if (coding_rate != SENTINEL)
    {
        i32_ret = ll_coding_rate_set(coding_rate);
        print_ll_ifc_error("ll_coding_rate_set", i32_ret);
    }
    if (tx_power != -96)
    {
        i32_ret = ll_tx_power_set(tx_power);
        print_ll_ifc_error("ll_tx_power_set", i32_ret);
    }
    if (preamble_len > 0)
    {
        i32_ret = ll_preamble_syms_set((uint16_t)(preamble_len));
        print_ll_ifc_error("ll_preamble_syms_set", i32_ret);
    }

    if(mac_get)
    {
        ll_mac_type_t mac_mode = MAC_INVALID;
        i32_ret = ll_mac_mode_get(&mac_mode);
        print_ll_ifc_error("ll_mac_mode_get", i32_ret);
        printf("MAC Mode = %d\n", mac_mode);
    }

    if (cmd_antenna_get)
    {
        uint8_t ant;
        i32_ret = ll_antenna_get(&ant);
        print_ll_ifc_error("ll_antenna_get", i32_ret);
        if (i32_ret >= 0)
        {
            printf("Antenna Configuration: %d\n", ant);
        }
    }

    if (cmd_antenna_set)
    {
        i32_ret = ll_antenna_set(antenna_cfg);
        print_ll_ifc_error("ll_receive_mode_set", i32_ret);
    }

    if (cmd_iq_inversion_set)
    {
        i32_ret = ll_iq_inversion_set(iq_inversion_cfg);
        print_ll_ifc_error("ll_iq_inversion_set", i32_ret);
    }

    if (cmd_sync_word)
    {
        i32_ret = ll_sync_word_set(sync_word_cfg);
        print_ll_ifc_error("ll_sync_word_set", i32_ret);
    }

    if(block_sleep)
    {
        i32_ret = ll_sleep_block();
        print_ll_ifc_error("ll_sleep_block", i32_ret);
    }
    if(unblock_sleep)
    {
        i32_ret = ll_sleep_unblock();
        print_ll_ifc_error("ll_sleep_unblock", i32_ret);
    }

    if((cmd_irq_flags_get) || (cmd_irq_flags_clear))
    {
        uint32_t irq_flags_to_clear = 0;
        uint32_t irq_flags_read = 0;
        uint8_t len = 4;

        if (cmd_irq_flags_clear)
        {
            len = strlen(buf)/2;
            buffer_to_hex(buf);
            irq_flags_to_clear = ((uint32_t)buf[3]);
            irq_flags_to_clear |= ((uint32_t)buf[2]) << 8;
            irq_flags_to_clear |= ((uint32_t)buf[1]) << 16;
            irq_flags_to_clear |= ((uint32_t)buf[0]) << 24;
        }

        // printf("\tirq_flags_to_clear = 0x%08X\n", irq_flags_to_clear);

        if (len != 4)
        {
            printf("Argument must be 32-bit hex\n");
        }
        else
        {
            // Check what flags are set, and clear some if we want
            i8_ret = ll_irq_flags(irq_flags_to_clear, &irq_flags_read);
            if (i8_ret<0)
            {
                print_ll_ifc_error("ll_irq_flags", i8_ret);
            }
            else
            {
                if(cmd_irq_flags_get)
                {
                    printf("irq_flags = 0x%08X\n", irq_flags_read);
                    print_irq_flags_text(irq_flags_read);
                }
            }
        }
    }

    if (uuid)
    {
        print_uuid();
    }
    if (eui64)
    {
        print_eui64();
    }

    if (delete_settings)
    {
        i32_ret = ll_settings_delete();
        print_ll_ifc_error("ll_settings_delete", i32_ret);
    }
    if (store_settings)
    {
        i32_ret = ll_settings_store();
        print_ll_ifc_error("ll_settings_store", i32_ret);
    }
    if (restore_defaults)
    {
        i32_ret = ll_restore_defaults();
        print_ll_ifc_error("ll_restore_defaults", i32_ret);
    }

    if (transmit_cw)
    {
        i32_ret = ll_transmit_cw();
        print_ll_ifc_error("ll_transmit_cw", i32_ret);
    }

    if (get_radio_params)
    {
        uint8_t sf, cr, bw;
        uint32_t freq;
        uint16_t preamble_len;
        uint8_t header_enabled, crc_enabled, iq_inverted;
        int8_t pwr;
        uint8_t sync_word;
        int8_t ret = ll_radio_params_get(&sf, &cr, &bw, &freq, &preamble_len,
                                         &header_enabled, &crc_enabled, &iq_inverted);
        print_ll_ifc_error("ll_radio_params_get", ret);
        if (ret >= 0)
        {
            float bw_khz;
            switch (bw) {
                case 0: bw_khz = 62.5; break;
                case 1: bw_khz = 125.0; break;
                case 2: bw_khz = 250.0; break;
                case 3: bw_khz = 500.0; break;
                default: bw_khz = -1.0; break;
            }
            printf("spreading factor %d\n", sf);
            printf("coding rate %d\n", cr);
            printf("frequency %lu\n", (unsigned long) freq);
            printf("bandwidth %f kHz\n", bw_khz);
            printf("preamble length %d symbols\n", preamble_len);
            printf("header %s enabled\n", header_enabled ? "is" : "is not");
            printf("crc %s enabled\n", crc_enabled ? "is" : "is not");
            printf("iq %s inverted\n", iq_inverted ? "is" : "is not");
        }
        ret = ll_tx_power_get(&pwr);
        print_ll_ifc_error("ll_tx_power_get", ret);
        if (ret >= 0)
        {
            printf("tx power %d dBm\n", pwr);
        }
        ret = ll_sync_word_get(&sync_word);
        print_ll_ifc_error("ll_sync_word_get", ret);
        if (ret >= 0)
        {
            printf("sync word: 0x%02x\n", sync_word);
        }
    }

    if (reset_mcu)
    {
        printf("Resetting module\n");
        i32_ret = ll_reset_mcu();
        print_ll_ifc_error("ll_reset_mcu", i32_ret);
    }

    if (bootloader_mode)
    {
        printf("Putting module in bootloader mode\n");
        i32_ret = ll_bootloader_mode();
        print_ll_ifc_error("ll_bootloader_mode", i32_ret);
    }

#if 0
    if (setting_encryption_key)
    {
        printf("Setting the encryption key\n");
        buffer_to_hex(buf);
        i32_ret = ll_encryption_key_set((uint8_t*)buf, strlen(buf));
        print_ll_ifc_error("ll_set_encryption_key", i32_ret);
    }
#endif

    switch (mode)
    {
        case MODE_NONE:
            break;
        case MODE_SLEEP:
            i32_ret = ll_sleep();
            print_ll_ifc_error("ll_sleep", i32_ret);
            break;
        case MODE_ECHO:
            i32_ret = ll_echo_mode();
            print_ll_ifc_error("ll_echo_mode", i32_ret);
            break;
        case MODE_ECHO_TEST:
            echo_test_mode();
            break;
        case MODE_ECHO_TX_PAYLOAD:
            echo_tx_with_payload((uint8_t *)buf, strlen(buf));
            break;
        case MODE_RX:
            rx_mode_single();
            break;
        case MODE_RX_CONT:
            rx_mode_cont(receive_time_ms, rx_cont_freq_err_requested);
            break;
        case MODE_TX:
            if(tx_payload_is_hex)
            {
                // Can't run strlen on the buf after you convert it to hex !
                uint8_t len = strlen(buf)/2;
                buffer_to_hex(buf);
                i32_ret = ll_packet_send_queue((uint8_t *)buf, len);
                print_ll_ifc_error("ll_packet_send_queue", i32_ret);
            }
            else
            {
                i32_ret = ll_packet_send_queue((uint8_t *)buf, strlen(buf));
                print_ll_ifc_error("ll_packet_send_queue", i32_ret);
            }

            if ((i32_ret < 0) || (i32_ret > 1))
            {
                fprintf(stderr, "Failed to send packet: %d\n", i32_ret);
                exit(EXIT_FAILURE);
            } else if (i32_ret == 0) {
                printf("Message sent (send queue now full)\n");
            } else {
                printf("Message sent\n");
            }
            break;
        default:
            fprintf(stderr, "ERROR: Invalid mode %d\n", mode);
    }

    MS_SLEEP(10);

    return 0;
}
