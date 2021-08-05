#ifndef API_H_
#define API_H_

#define PROTOCOL_VERSION 0

// client to server
#define TYPE_REQUEST 0
#define TYPE_SHUTDOWN 1
#define TYPE_PING 3
//server to client
#define TYPE_RESPONSE 2

struct message_header {
	uint8_t protocol_version;
	uint8_t type;
} __attribute__((packed));

#define REQUEST_DESTINATION_FILE 0
#define REQUEST_DESTINATION_SOCKET 1

struct request {
	uint32_t center_freq;
	uint32_t sampling_rate;
	uint32_t band_freq;
	uint8_t destination;
} __attribute__((packed));

#define RESPONSE_STATUS_SUCCESS 0
#define RESPONSE_STATUS_FAILURE 1

#define RESPONSE_DETAILS_INVALID_REQUEST 1
#define RESPONSE_DETAILS_OUT_OF_BAND_FREQ 2
#define RESPONSE_DETAILS_INTERNAL_ERROR 3

struct response {
	uint8_t status;
    uint32_t details; // on success contains file index, on error contains error code
} __attribute__((packed));


#endif /* API_H_ */
