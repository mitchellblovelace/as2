#include "hal/udp.h"
#include "hal/sampler.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define UDP_PORT 12345
#define BUFFER_SIZE 1500

static pthread_t udp_thread_id;
static int socket_descriptor;
static volatile bool is_initialized = false;
static char last_command[BUFFER_SIZE] = "";
extern volatile bool program_running; 

static void* udp_thread_function(void* arg) {
    (void)arg;
    struct sockaddr_in sin; // sin is the socket address structure for the UDP socket
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(UDP_PORT);

    socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    bind(socket_descriptor, (struct sockaddr*)&sin, sizeof(sin));
    struct timeval timeout = {1, 0};
    setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in sinRemote;
    unsigned int sinRemoteLen = sizeof(sinRemote);
    char message_rx[BUFFER_SIZE];

    while (is_initialized) {
        int bytes_rx = recvfrom(socket_descriptor, message_rx, BUFFER_SIZE - 1, 0,
                                        (struct sockaddr*)&sinRemote, &sinRemoteLen);

        if (bytes_rx <= 0) {
            continue; // ignore errors and continue listening
        }

        message_rx[bytes_rx] = '\0'; // null-terminate the received message

        // handle <enter> command 

        if (message_rx[0] == '\n' || message_rx[0] == '\r') {
            if (strlen(last_command) == 0) {
                char reply[] = "No previous command to execute.\n";
                sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
                continue;
            }

            strcpy(message_rx, last_command); // copy last command to message_rx
        } else {
           message_rx[strcspn(message_rx, "\r\n")] = 0; // remove trailing newline characters
           strcpy(last_command, message_rx); // store the last command
        }

        char reply[BUFFER_SIZE] = "";

        // parse command 
        if (strncmp(message_rx, "help", 4) == 0 || strncmp(message_rx, "?", 1) == 0) {
            snprintf(reply, BUFFER_SIZE, "Accepted commands:\ncount\nlength\ndips\nhistory\nstop\n<enter>\n");
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
        } else if (strncmp(message_rx, "count", 5) == 0) {
            //long long num_samples = Sampler_getNumSamplesTaken(); 
            snprintf(reply, BUFFER_SIZE, "Total samples taken: %lld\n", Sampler_getNumSamplesTaken());
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
        } else if (strncmp(message_rx, "length", 6) == 0) {
            //int history_size = Sampler_getHistorySize();
            snprintf(reply, BUFFER_SIZE, "Number of samples in history: %d\n", Sampler_getHistorySize());
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
        } else if (strncmp(message_rx, "dips", 4) == 0) {

            int size = 0;
            double* history = Sampler_getHistory(&size);
            double curr_avg = Sampler_getAverageReading();

            int dips = Sampler_getDips(history, size, curr_avg);
            snprintf(reply, BUFFER_SIZE, "Number of dips detected: %d\n", dips);
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
            free(history); // free the allocated memory for history
            
        } else if (strncmp(message_rx, "history", 7) == 0) {
            int size = 0;
            double* history = Sampler_getHistory(&size);

            char chunk[BUFFER_SIZE] = "";
            int offset = 0;

            // formatting 
            for (int i = 0; i < size; i++) {
                char temp[32];

                if ((i+1) % 10 == 0 || i == size - 1) {
                    snprintf(temp, sizeof(temp), "%.3f, \n", history[i]);
                } else {
                    snprintf(temp, sizeof(temp), "%.3f, ", history[i]);
                }

                if (offset + strlen(temp) >= BUFFER_SIZE) {
                    sendto(socket_descriptor, chunk, strlen(chunk), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
                    offset = 0;
                    chunk[0] = '\0';
                }

                strcat(chunk, temp);
                offset += strlen(temp);
            }

            if (offset > 0) {
                sendto(socket_descriptor, chunk, strlen(chunk), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
            }

            free(history); // free the allocated memory for history

        } else if (strncmp(message_rx, "stop", 4) == 0) {
            snprintf(reply, BUFFER_SIZE, "Stopping UDP listener.\n");
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
            program_running = false; // signal main loop to stop
        } else {
            snprintf(reply, BUFFER_SIZE, "Unknown command: %.100s\n", message_rx);
            sendto(socket_descriptor, reply, strlen(reply), 0, (struct sockaddr*)&sinRemote, sinRemoteLen);
        }




    }

    return NULL;


}



void UDP_init(void) {
    is_initialized = true;
    pthread_create(&udp_thread_id, NULL, udp_thread_function, NULL);
}

void UDP_cleanup(void) {
    is_initialized = false;
    pthread_join(udp_thread_id, NULL);
    close(socket_descriptor);
}