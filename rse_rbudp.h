#pragma once
#include <cstdint>
#include <cmath>
#include "rse_debug.h"
#include "rse_ds.h"
#include "rse_io.h"
#include "rse_sockets.h"

namespace rse {

    // Reliable Blast UDP
    namespace rbudp {

        constexpr int PACKET_HEADER_SIZE = 4; // in bytes

        constexpr int MAX_DATAGRAM_SIZE = 65536;
        constexpr int ASSUMED_PORT_SIZE = 65536;

        // The block size should be a power of 2 and less than 65536
        // since that is the max size a udp datagram can be.

        constexpr int DEFAULT_BLOCK_SIZE = 4096;
        constexpr int PATH_SIZE = 2048; // includes null terminator

        // A packet consists of a header which is 16 bytes.
        // The packet header consists of:
        //      The first 4 bytes is the header ID. Which is unsigned 32 bit integer.
        // The packet body is user defined. By default it is 4kb
        
        struct PacketHeader {
            uint32_t id;
        };

        struct TransmissionInfo {
            uint32_t number_packets = 0;
            uint32_t block_size = 0; // in bytes. Does not include the 4 byte header to a packet.
            uint32_t packet_size = 0; // packet size which is the block size + the packet header size
            uint32_t bitmap_size = 0; // (number of packets / 8) + 1
            uint32_t summation_block_size; // summation of all blocks for every packet
            uint32_t total_transmission_size = 0;
            uint32_t max_packets_per_transmission = 0; // the max number of packets that can be sent given a port has a max size of 65536
            char path_name[PATH_SIZE]; // file path that you want to write to. Must include null terminator
        };
                
        struct ReceiverSockets {
            sk::SocketHandle socket_udp;
            sk::SocketHandle socket_listen;
            sk::SocketHandle socket_sender;
        };

        struct SenderSockets {
            sk::SocketHandle socket_receiver;
            sk::SocketHandle socket_udp;
        };


        bool ReceiveConnections(const char* hostname, const char* port, int port_num, ReceiverSockets& out) {
            
            sk::SocketHandle& socket_udp = out.socket_udp;
            sk::SocketHandle& socket_listen = out.socket_listen;
            sk::SocketHandle& socket_sender = out.socket_sender;

            debug_printf("[receiver]: creating udp socket\n");
            socket_udp = sk::CreateUDPSocketReceiver(port_num);
            if (sk::IsInvalidSocket(socket_udp)) { debug_printf("[receiver]: failed to create udp socket\n"); return false; }

            debug_printf("[receiver]: creating listen socket\n");
            socket_listen = rse::sk::CreateListenSocket(hostname, port, true);
            if (sk::IsInvalidSocket(socket_listen)) {
                sk::CloseSocket(socket_udp);
                return false;
            }

            // Will wait until it connects
            debug_printf("[receiver]: waiting for connection...\n");
            socket_sender = sk::AcceptFirstConnectionOnListenSocket(socket_listen);

            return true;
        }

        bool ReceiveTransmissionInfoAndReply(const ReceiverSockets& in, TransmissionInfo& info) {

            sk::SocketHandle socket_sender = in.socket_sender;
            sk::SocketError result;
            info = { 0 };

            // First 4 bytes are the number packets.
            // Next 4 bytes are the size of the packets.
            debug_printf("[receiver]: waiting to receive tranmission header...\n");
           
            result = sk::Recv(socket_sender, (char*)&info.number_packets, 4, 0);
            if (sk::IsError(result)) { return false; }
            result = sk::Recv(socket_sender, (char*)&info.block_size, 4, 0);
            if (sk::IsError(result)) { return false; }
            result = sk::Recv(socket_sender, info.path_name, rbudp::PATH_SIZE, 0);
            if (sk::IsError(result)) { return false; }

            info.bitmap_size = (info.number_packets / 8) + 1;
            info.packet_size = info.block_size + PACKET_HEADER_SIZE;
            info.total_transmission_size = info.number_packets * info.block_size;
            info.summation_block_size = info.block_size * info.number_packets;
            info.max_packets_per_transmission = ASSUMED_PORT_SIZE / info.packet_size;

            debug_printf("[receiver]: transmission info [%d][%d][%s]\n", info.number_packets, info.block_size, info.path_name);

            // Send a reply saying to start transmission
            debug_printf("[receiver]: sending reply to start transmission\n");
            uint8_t flag = 1;
            result = sk::Send(socket_sender, (char*)&flag, sizeof(flag), 0);
            if (sk::IsError(result)) return false;

            return true;
        }

        bool ReceiveFile(const ReceiverSockets &rc_sockets, const TransmissionInfo &handshake) {

            sk::SocketError result;
            sk::SocketHandle socket_udp = rc_sockets.socket_udp;
            sk::SocketHandle socket_sender = rc_sockets.socket_sender;
            timeval tval = { 0 };
            int len = sizeof(sockaddr_in);

            // Create a new file and memory map
            io::MemMap memmap;
            if (!io::MapMemory(handshake.path_name, handshake.summation_block_size, io::MemMapIO::READ_WRITE, memmap)) {
                debug_printf("failed to memory map path [%s]\n", handshake.path_name);
                return false;
            }

            rse::Bitmap packet_bitmap(handshake.number_packets);
            char* packet_buffer = new char[handshake.packet_size];
            bool return_val = false;
            while (true) {

                // read message signifing the sender is done
                debug_printf("[receiver]: waiting for go ahead from sender...\n");
                uint8_t flag;
                result = sk::Recv(socket_sender, (char*)&flag, sizeof(flag), 0);
                if (sk::IsError(result)) break;

                debug_printf("[receiver]: sender is telling me it sent udp stuff\n");
                if (flag == 0) {
                    // the sender is done 
                    return_val = true;
                    debug_printf("[receiver]: sender told me it's happy with transmission and has finished\n");
                    break;
                }

                // Check if udp socket is ready to be read
                fd_set read_set;
                FD_ZERO(&read_set);
                FD_SET(socket_udp, &read_set);

                // Check for udp messages
                while (select(0, &read_set, nullptr, nullptr, &tval) > 0) {

                    sockaddr_in cliaddr = { 0 };

                    debug_printf("[receiver]: recvfrom sender\n");
                    memset(packet_buffer, 0, handshake.packet_size);
                    result = sk::RecvFrom(socket_udp, packet_buffer, handshake.packet_size,
                        0, (sockaddr*)&cliaddr,
                        &len);

                    if (sk::IsError(result)) {
                        debug_printf("[receiver]: error reading packet\n");
                        goto label_cleanup;
                    } 
                    else {
                        uint32_t id = *(uint32_t*)packet_buffer;
                        // This check ensures that the data we access via the 
                        // bitmap is valid
                        if (id >= handshake.number_packets) {
                            debug_printf("[receiver]: packet error\n");
                            goto label_cleanup;
                        }

                        char* block_ptr = packet_buffer + rbudp::PACKET_HEADER_SIZE;

                        // Copy the packet buffer to the mapped file
                        char* mem_ptr = (char*)memmap.ptr + (id * (size_t)handshake.block_size);
                        memcpy(mem_ptr, block_ptr, handshake.block_size);

                        debug_printf("[receiver]: block [%c]\n", block_ptr[0]);

                        packet_bitmap.Set(id);

                        debug_printf("[receiver]: read packet [%d]\n", id);
                        debug_printf("[receiver]: bitmap ");
                        packet_bitmap.Print();
                    }
                    debug_printf("[receiver]: selecting...\n");
                }

                debug_printf("[receiver]: no more packets to read\n");

                debug_printf("[receiver]: sending off bitmap to sender\n");

                // send off our bitmap to the client.
                result = sk::Send(socket_sender, (char*)packet_bitmap.Data(), packet_bitmap.SizeOf(), 0);
                if (sk::IsError(result)) break;
            }

        label_cleanup:

            delete[] packet_buffer;
            io::UnmapMemory(memmap);
            return return_val;
        }

        // This does require that sockets have been initialised.
        // Wait to receive a file from the specifed host at the specified port
        // The reason this is a long function is because its easier to not make a mistake that way
        // particularly in terms of security. Ideally the whole thing would just be one long function.
        // Its up for debate how it should get split up.
        bool WaitToReceive(const char* hostname, const char* port_str, int port_num) {

            ReceiverSockets rc_sockets;
            TransmissionInfo handshake = { 0 };

            if (!rbudp::ReceiveConnections(hostname, port_str, port_num, rc_sockets)) {
                debug_printf("[receiver]: receiving connections failed\n");
                return false;
            }
            sk::SocketHandle socket_udp = rc_sockets.socket_udp;
            sk::SocketHandle socket_listen = rc_sockets.socket_listen;
            sk::SocketHandle socket_sender = rc_sockets.socket_sender;

            if (!rbudp::ReceiveTransmissionInfoAndReply(rc_sockets, handshake)) {
                rse::sk::CloseSocket(socket_udp);
                rse::sk::CloseSocket(socket_listen);
                rse::sk::CloseSocket(socket_sender);
                debug_printf("[receiver]: receiving transmission failed\n");
                return false;
            }

            bool ret_val = rbudp::ReceiveFile(rc_sockets, handshake);

            debug_printf("[receiver]: finished\n");
            rse::sk::CloseSocket(socket_udp);
            rse::sk::CloseSocket(socket_listen);
            rse::sk::CloseSocket(socket_sender);

            return ret_val;
        }

        bool SenderConnect(const char* hostname, const char* port_str, SenderSockets& s_sockets) {

            sockaddr serverAddr;
            socklen_t serverAddrLen = 0;
            s_sockets.socket_receiver = sk::CreateClientSocketForServer(hostname, port_str, serverAddr, serverAddrLen, true);

            debug_printf("[sender]: connecting to receiver\n");
            sk::SocketError result = sk::Connect(s_sockets.socket_receiver, &serverAddr, serverAddrLen);
            if (sk::IsError(result)) {
                sk::CloseSocket(s_sockets.socket_receiver);
                debug_printf("[sender]: failed to connect\n");
                return false;
            }

            s_sockets.socket_udp = rse::sk::CreateUDPSocketSender();
            if (sk::IsInvalidSocket(s_sockets.socket_udp)) {
                debug_printf("[sender]: invalid udp socket\n");
                sk::CloseSocket(s_sockets.socket_receiver);
                return false;
            }

            return true;
        }

        bool SendTransmissionInfoAndWait(
            const SenderSockets& s_sockets,
            const char* path_to_write, size_t send_file_size, const int block_size,
            TransmissionInfo& handshake) {

            sk::SocketError result;
            handshake = { 0 };

            handshake.number_packets = (send_file_size / block_size) + 1;
            handshake.block_size = block_size;
            handshake.packet_size = block_size + PACKET_HEADER_SIZE;
            handshake.bitmap_size = (handshake.number_packets / 8) + 1;
            handshake.max_packets_per_transmission = ASSUMED_PORT_SIZE / handshake.packet_size;
            strcpy(handshake.path_name, path_to_write);

            // Send off the packet info to the receiver
            debug_printf("[sender]: sending handshake...\n");
            result = sk::Send(s_sockets.socket_receiver, (char*)&handshake.number_packets, 4, 0);
            if (sk::IsError(result)) return false;
            result = sk::Send(s_sockets.socket_receiver, (char*)&handshake.block_size, 4, 0);
            if (sk::IsError(result)) return false;
            result = sk::Send(s_sockets.socket_receiver, handshake.path_name, rse::rbudp::PATH_SIZE, 0);
            if (sk::IsError(result)) return false;

            // Wait for a response from the receiver
            debug_printf("[sender] sender waiting for response from receiver...\n");
            uint8_t is_receiver_happy;
            result = sk::Recv(s_sockets.socket_receiver, (char*)&is_receiver_happy, sizeof(is_receiver_happy), 0);
            if (sk::IsError(result)) {
                debug_printf("Error getting flag\n");
                return false;
            }

            // Flag siginifies we should start protocol
            debug_printf("[sender] receiver is happy with handshake\n");
            return true;
        }


        bool SendPackets(const TransmissionInfo &handshake, SenderSockets s_sockets,
            const uint32_t block_size,
            const char* filename, const char* hostname, int port_num, size_t send_file_size) {

            sk::SocketError result;
            bool return_val = false;

            // Filling server information for use with a udp socket
            sockaddr_in servaddr;
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port_num);
            servaddr.sin_addr.s_addr = inet_addr(hostname);

            // Memory map our file we want to send 
            rse::io::MemMap memmap;
            if (!rse::io::MapMemory(filename, send_file_size, rse::io::MemMapIO::READ_ONLY, memmap)) {
                debug_printf("[sender]: failed to mem map file");
                return false;
            }

            rse::Bitmap recv_bitmap(handshake.number_packets);
            char* packet_buffer = new char[handshake.packet_size];
            char* recv_bitmap_buffer = new char[handshake.bitmap_size];
            uint32_t sent_packets = 0;

            // Keep sending until our bitmap is fully set
            while (true) {

                debug_printf("[sender]: sending udp payload\n");
                sent_packets = 0;

                for (uint32_t i = 0; i < recv_bitmap.Size(); i++) {

                    if (sent_packets >= handshake.max_packets_per_transmission) break;

                    if (!recv_bitmap[i]) {
                        sent_packets++;

                        debug_printf("[sender]: sending packet [%d]\n", i);

                        uint32_t offset_start = i * block_size;
                        uint32_t offset_end = offset_start + block_size;
                        if (offset_end > send_file_size) offset_end = send_file_size;
                        uint32_t send_size = offset_end - offset_start;

                        memset(packet_buffer, 0, handshake.packet_size);
                        // Copy packet header into packet buffer
                        uint32_t* header_ptr = (uint32_t*)packet_buffer;
                        *header_ptr = i;
                        // Copy block data into packet buffer
                        char* block_file_ptr = (char*)memmap.ptr + offset_start;
                        char* block_mem_ptr = packet_buffer + PACKET_HEADER_SIZE;

                        memcpy(block_mem_ptr, block_file_ptr, send_size);

                        result = rse::sk::SendTo(s_sockets.socket_udp, packet_buffer, handshake.packet_size, 0, (const sockaddr*)&servaddr, sizeof(servaddr));
                        if (rse::sk::IsError(result)) {
                            rse::sk::ErrorMessage("[sender]: sendto failed");
                            goto label_cleanup;
                        }
                    }
                }

                // Send a message telling the receiver we are done 
                debug_printf("[sender]: telling receiver I am done\n");
                uint8_t flag = 1;
                sk::Send(s_sockets.socket_receiver, (char*)&flag, sizeof(flag), 0);

                //Check if everything sent correctly.
                debug_printf("[sender]: waiting for bitmap...\n");
                result = sk::Recv(s_sockets.socket_receiver, recv_bitmap_buffer, handshake.bitmap_size, 0);
                if (rse::sk::IsError(result)) {
                    debug_printf("[sender] error getting bitmap\n");
                    goto label_cleanup;
                }
                // Copy buffer directly into bitmap struct so we can have a look at it.
                memcpy(recv_bitmap.Data(), recv_bitmap_buffer, handshake.bitmap_size);

                debug_printf("[sender]: received bitmap ");
                recv_bitmap.Print();

                bool has_sent_all_flag = true;
                for (size_t i = 0; i < recv_bitmap.Size(); i++) {
                    if (!recv_bitmap[i]) {
                        has_sent_all_flag = false;
                        break;
                    }
                }

                if (has_sent_all_flag) {
                    return_val = true;
                    break;              
                }
            }

        label_cleanup:

            delete[] recv_bitmap_buffer;
            delete[] packet_buffer;

            rse::io::UnmapMemory(memmap);

            return return_val;
        }

        // USe null terminated strings obviously
        // Path size must be less than PATH_SIZE
        // Sockets must be initialised
        // block size must be a power of 2
        bool SendFile(const char* filename, 
            const char* path_to_write, const char* hostname, const char* port_str, int port_num, const int block_size = DEFAULT_BLOCK_SIZE) {
           
            TickTock a;

            a = Tick();
            size_t path_size = strlen(path_to_write);
            if (path_size >= PATH_SIZE) {
                debug_printf("Path size is too large\n");
                return false;
            }

            debug_printf("[sender]: starting...\n");

            // Open the file we want to send and work out how big it is
            FILE* file = fopen(filename, "rb");
            fseek(file, 0L, SEEK_END);
            size_t send_file_size = ftell(file);
            fclose(file);

            SenderSockets send_sockets;
            if (!SenderConnect(hostname, port_str, send_sockets)) {
                return false;
            }
            debug_printf("[sender]: connection time [%lf]\n", Tock(a));

            a = Tick();
            // Specify how many packets we want to send along with the size of their payloads.
            // also calculate the size of the bitmap required to keep track of all the packets.
            TransmissionInfo handshake = { 0 };
            if (!SendTransmissionInfoAndWait(send_sockets, path_to_write, send_file_size, block_size, handshake)) {
                sk::CloseSocket(send_sockets.socket_receiver);
                sk::CloseSocket(send_sockets.socket_udp);
                return false;
            }
            debug_printf("[sender]: Handshake time [%lf]\n", Tock(a));

            a = Tick();
            bool ret_val = SendPackets(handshake, send_sockets, block_size, filename, hostname, port_num, send_file_size);
            debug_printf("[sender]: Send time [%lf]\n", Tock(a));

            debug_printf("[sender]: telling sender I am finished\n");
            uint8_t flag = 0;
            rse::sk::Send(send_sockets.socket_receiver, (char*)&flag, sizeof(flag), 0);

            sk::CloseSocket(send_sockets.socket_receiver);
            sk::CloseSocket(send_sockets.socket_udp);
            return ret_val;
        }

    }

}