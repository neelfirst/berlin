#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <cassert>
#include <jpeglib.h>
#include <math.h>

struct in_addr ipAddress;
int _socket;
const uint16_t udp_port = 2368;
const int CLOSED_SOCKET = -1;
const double phi[32] = {-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,-25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,-20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,-14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67};

int main()
{
    assert(UINT32_MAX >= 4294967295);
    inet_aton("192.168.1.201", &ipAddress);
    /*
     * Can't seem to use SOCK_NONBLOCK here because that
     * makes recvfrom() return every time with EAGAIN error.
     */
    _socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (_socket == -1) {
        std::cerr << "socket() failed with errno "
                  << errno
                  << std::endl;

        _exit(1);
    };

    int socketOptionValue = 1;
    socklen_t socketOptionLength = (socklen_t) sizeof(socketOptionValue);
    int result;
    result = setsockopt(
        _socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &socketOptionValue,
        sizeof socketOptionValue
        );
    if (result == -1) {
        close(_socket);
        _socket = CLOSED_SOCKET;
        _exit(2);
    };

    /*
     * Retrieve the receive buffer default size, for logging purposes,
     * and then increase the size of the receive buffer to at least
     * one second's worth of LaserPackets.
     */
    socketOptionValue = 0;
    result = getsockopt(
        _socket,
        SOL_SOCKET,
        SO_RCVBUF,
        &socketOptionValue,
        &socketOptionLength
        );
    if (result == 0) {
        std::cout << "UDP receive buffer default size is "
                  << socketOptionValue
                  << std::endl;
    } else {
        std::cerr << "getsockopt() failed to retrieve UDP default receive buffer size"
                  << std::endl;
    };
    socketOptionValue = 1206 * 1808;
    result = setsockopt(
        _socket,
        SOL_SOCKET,
        SO_RCVBUF,
        &socketOptionValue,
        sizeof socketOptionValue
        );
    if (result == 0) {
        std::cout << "UDP receive buffer size set to "
                  << socketOptionValue
                  << std::endl;
    } else {
        std::cerr << "setsockopt() failed to set UDP receive buffer size to "
                  << socketOptionValue
                  << std::endl;
    };


    sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(udp_port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_socket, (sockaddr *) (&my_addr), sizeof(sockaddr_in)) == -1)
    {
        std::cerr << "bind() failed with errno "
                  << errno
                  << std::endl;
        _exit(3);
    }

    struct sockaddr source_address;
    socklen_t source_address_length = sizeof(source_address);

    char lidarBuffer[1206];
    uint16_t blk_id, theta, r;
    uint8_t intensity;
    int bytesReceived;
    /*
     * Enter a loop which does a blocking read() for the next
     * datagram, gets the system time as soon as that read returns
     * and then does all of the analysis.
     */
    while ( true )
    {
        /*
         * Block until a datagram or an error is returned.
         */
        bytesReceived = recvfrom(
            _socket,
            lidarBuffer,
            1206,
            0,
            &source_address,
            &source_address_length
            );

        /*
         * If there was an error, return to the read the next datagram
         * immediately, without updating any time records.
         */
        if (bytesReceived == -1) {
            std::cerr << "recvfrom() failed with errno: "
                      << errno
                      << ", "
                      << strerror(errno)
                      << std::endl;
            continue;
        };

        if (source_address.sa_family == AF_INET)
	{
            sockaddr_in * socket_address = reinterpret_cast<sockaddr_in *>(&source_address);
            if (socket_address->sin_addr.s_addr == ipAddress.s_addr)
	    {
                if (bytesReceived > 1)
		{
                    /*
                     * As well as can be determined, a complete datagram was
                     * received.
                     */
		    std::cout << "begin packet" << std::endl;
		    for (int j = 0; j < 12; j++)
		    {
			memcpy(&blk_id, &(lidarBuffer[j*100]), 2);
			memcpy(&theta, &(lidarBuffer[(j*100)+2]), 2);
			std::cout << "blk_id = " << blk_id << " theta = " << theta << std::endl;

			for (int i = 0; i < 32; i++)
			{
			    memcpy(&r, &(lidarBuffer[(j*100+4)+(i*3)]), 2);
			    memcpy(&intensity, &(lidarBuffer[(j*100+4)+(i*3+2)]), 1);
			    std::cout << "r = " << r << " i = " << unsigned(intensity) << std::endl;

			    // convert theta/phi to x & y
			    double x = cos(phi[i])*sin(theta) + 1;
			    double y = cos(phi[i])*cos(theta) + 1;
			    // scale x & y to image pixel dimensions
			    int X = x*200; int Y = y*250;
			    // copy image1 to image3, overwrite image3 with image2 pixels
			}
		    }
                    continue;
                }
		else std::cerr << "Got a partial packet" << std::endl;
            }
	else std::cerr << "Got a packet from the wrong source" << std::endl;
        };
    };
};

