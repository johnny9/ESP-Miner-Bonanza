idf_component_register(
SRCS
    "utils.c"
                        "hash_bytes.c"
    "mining.c"
    "mining_job.c"
    "stratum_protocol.c"
    "sv1_protocol.c"
    "sv1_client.c"
    "stratum_socket.c"
    "coinbase_decoder.c"
    "segwit_addr.c"
    "base58.c"
    "miner_job.c"

INCLUDE_DIRS
    "include"

REQUIRES
    "mining_job"
    "mbedtls"
    "app_update"
    "esp_timer"
    "tcp_transport"
    "esp_netif"
    "esp_psram"
)
