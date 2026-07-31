/**
  * @file           : ethernet_sbc_protocol.cpp
  * @brief          : High-Speed LwIP TCP Socket Interface for SBC / AI Edge Compute
  *                   Streams High-Frequency Sensor Metrics to Edge AI Coprocessors
  ******************************************************************************
  */

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdio.h>

/* --- SBC EDGE COMPUTE NETWORK CONFIGURATION --- */
#define SBC_AI_SERVER_IP   "192.168.1.200" // IP of Jetson / Pi Edge AI Node
#define SBC_AI_SERVER_PORT 5005            // TCP Port for AI Telemetry Stream
#define TX_BUFFER_SIZE     512

static int sbc_socket_fd = -1;

/**
  * @brief Establishes persistent Socket connection to Edge AI SBC
  */
int Ethernet_SBC_Connect(void) {
    struct sockaddr_in sbc_addr;

    sbc_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sbc_socket_fd < 0) {
        return -1; // Socket Creation Failed
    }

    memset(&sbc_addr, 0, sizeof(sbc_addr));
    sbc_addr.sin_family = AF_INET;
    sbc_addr.sin_port = htons(SBC_AI_SERVER_PORT);
    sbc_addr.sin_addr.s_addr = inet_addr(SBC_AI_SERVER_IP);

    if (connect(sbc_socket_fd, (struct sockaddr*)&sbc_addr, sizeof(sbc_addr)) < 0) {
        close(sbc_socket_fd);
        sbc_socket_fd = -1;
        return -2; // SBC Connection Refused
    }

    return 0; // Success
}

/**
  * @brief Transmits high-frequency sensor matrix to Edge AI server for TFLite inference
  */
int Ethernet_SBC_StreamTelemetry(float temp, float ph, float do_val, float od, uint32_t timestamp_ms) {
    if (sbc_socket_fd < 0) {
        if (Ethernet_SBC_Connect() != 0) {
            return -1; // Retry Connection Failed
        }
    }

    char tx_payload[TX_BUFFER_SIZE];
    int payload_len = snprintf(tx_payload, sizeof(tx_payload),
        "{\"ts\":%lu,\"temp\":%.2f,\"ph\":%.2f,\"do\":%.2f,\"od\":%.4f,\"node_id\":\"STM32H7_BIO01\"}\n",
        timestamp_ms, temp, ph, do_val, od);

    int sent_bytes = send(sbc_socket_fd, tx_payload, payload_len, 0);
    if (sent_bytes < 0) {
        close(sbc_socket_fd);
        sbc_socket_fd = -1; // Force Reconnect on next cycle
        return -2;
    }

    return sent_bytes;
}

/*  custon application processor code yet to be developeed 
  */
int Ethernet_SBC_ReceiveInference(float *predicted_growth_rate, float *feed_rate_ml_min) {
    if (sbc_socket_fd < 0) return -1;

    char rx_buf[128];
    int rx_bytes = recv(sbc_socket_fd, rx_buf, sizeof(rx_buf) - 1, MSG_DONTWAIT);
    
    if (rx_bytes > 0) {
        rx_buf[rx_bytes] = '\0';
        // Parse incoming AI recommendation string (Format: "GROWTH:0.85,FEED:1.2")
        sscanf(rx_buf, "GROWTH:%f,FEED:%f", predicted_growth_rate, feed_rate_ml_min);
        return 0;
    }

    return -1; // No new inference payload
}
