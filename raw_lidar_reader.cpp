#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <cassert>
#include <jpeglib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

struct in_addr ipAddress;
int _socket;
const uint16_t udp_port = 2368;
const int CLOSED_SOCKET = -1;

struct pt {uint16_t w=0, h=0, forget=0;};
const std::string FILENAME = "berkeley.csv";
const double phi[32] = {-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,-25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,-20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,-14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67};
const int BASEROWS = 1248; // -19->19 x 32
const int THETA_MIN = -19;
const int THETA_MAX = 19;
const double RTHRESHOLD = 0;
const double ITHRESHOLD = 0.2;
const int WTHICK = 10;
const int HTHICK = 14;
const int FORGET = 20;

int getIndex (double th, int phindex)
{
    int theta = int(th);
    int shift = 0 - THETA_MIN;
    theta += shift;
    return theta*32 + phindex;
}

int main()
{
//////// IGNORE ALL OF THIS BECAUSE IT WORKS PRETTY DAMN WELL THANKS BRYCE
    assert(UINT32_MAX >= 4294967295);
    inet_aton("192.168.1.201", &ipAddress);
    // Can't seem to use SOCK_NONBLOCK here because that makes recvfrom() return every time with EAGAIN error.
    _socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (_socket == -1)
    {
        std::cerr << "socket() failed with errno " << errno << std::endl;
        _exit(1);
    };

    int socketOptionValue = 1;
    socklen_t socketOptionLength = (socklen_t) sizeof(socketOptionValue);
    int result;
    result = setsockopt(_socket,SOL_SOCKET,SO_REUSEADDR,&socketOptionValue,sizeof socketOptionValue);
    if (result == -1) { close(_socket); _socket = CLOSED_SOCKET; _exit(2); };

    // Retrieve the receive buffer default size, for logging purposes, and then
    // increase the size of the receive buffer to at least one second's worth of LaserPackets.
    socketOptionValue = 0;
    result = getsockopt(_socket,SOL_SOCKET,SO_RCVBUF,&socketOptionValue,&socketOptionLength);
    if (result == 0) std::cout << "UDP receive buffer default size is " << socketOptionValue << std::endl;
    else std::cerr << "getsockopt() failed to retrieve UDP default receive buffer size" << std::endl;

    socketOptionValue = 1206 * 1808;
    result = setsockopt(_socket,SOL_SOCKET,SO_RCVBUF,&socketOptionValue,sizeof socketOptionValue);
    if (result == 0) std::cout << "UDP receive buffer size set to " << socketOptionValue << std::endl;
    else std::cerr << "setsockopt() failed to set UDP receive buffer size to " << socketOptionValue << std::endl;

    sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(udp_port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_socket, (sockaddr *) (&my_addr), sizeof(sockaddr_in)) == -1)
    {
        std::cerr << "bind() failed with errno " << errno << std::endl;
        _exit(3);
    }
    struct sockaddr source_address;
    socklen_t source_address_length = sizeof(source_address);
    char lidarBuffer[1206];
    int bytesReceived;
/////////////////////////////////////////////////////////


    // 1. import baseline information from csv
    // columns: 0 = theta, 1 = phi, 2 = radius
    float Rdata[BASEROWS][3];
    std::ifstream file(FILENAME.c_str());
    for (int row = 0; row < BASEROWS; row++)
    {
	std::string line;
	std::getline(file,line);
	if (!file.good()) break;
	std::istringstream iss(line);
	for (int col = 0; col < 3; col++)
	{
	    std::string val;
	    std::getline(iss,val,',');
	    std::istringstream convertor(val);
	    convertor >> Rdata[row][col];
	    iss.clear();
	}
    }
    file.close();

    // forget counter mechanics:
    // 0 = stay at 0
    // activated -> move from 0->1
    // >0 --> increment each round, ignore de/activations
    // FORGET -> move from FORGET->-FORGET & deactivate
    // <0 --> increment each round, ignore activations
    std::vector<pt> activePoints;

    // 2. use opencv to import two source images, perform dimension checking
    double previousRotationValue = 0, THETA = 0;
    uint16_t blk_id, theta, r; // uint8_t intensity;
    pt activePt, srcDims;
    int W, H;
    cv::Mat src1 = cv::imread("map.png", CV_LOAD_IMAGE_COLOR);
    cv::Mat src2 = cv::imread("sat.png", CV_LOAD_IMAGE_COLOR);
    cv::Mat src3 = cv::imread("map.png", CV_LOAD_IMAGE_COLOR);
    srcDims.w = src1.cols; srcDims.h = src1.rows;
    if (srcDims.w != src2.cols || srcDims.h != src2.rows)
    {
	std::cerr << "fatal: source image dimensional mismatch" << std::endl;
	_exit(4);
    }

    // Enter a loop which does a blocking read() for the next datagram
    // gets the system time as soon as that read returns
    // and then does all of the analysis.
    while ( true )
    {
        // Block until a datagram or an error is returned.
        bytesReceived = recvfrom(_socket,lidarBuffer,1206,0,&source_address,&source_address_length);
        // If there was an error, return to the read the next datagram immediately, without updating any time records.
        if (bytesReceived == -1)
	{
            std::cerr << "recvfrom() failed with errno: " << errno << ", " << strerror(errno) << std::endl;
            continue;
        };

        if (source_address.sa_family == AF_INET)
	{
            sockaddr_in * socket_address = reinterpret_cast<sockaddr_in *>(&source_address);
            if (socket_address->sin_addr.s_addr == ipAddress.s_addr)
	    {
//		timespec start, stop;
//		clock_gettime(CLOCK_REALTIME,&start);
                if (bytesReceived > 1) // 3. grab real time data from Lidar in loop (theta, R)
		{
                    // As well as can be determined, a complete datagram was received.
		    for (int j = 0; j < 12; j++)
		    {
			memcpy(&blk_id, &(lidarBuffer[j*100]), 2);
			memcpy(&theta, &(lidarBuffer[(j*100)+2]), 2);
			THETA = theta*0.01;
			if (THETA > 180) THETA -= 360;
			if (THETA < THETA_MIN || THETA > THETA_MAX) continue;
			for (int i = 0; i < 32; i++)
			{
			    memcpy(&r, &(lidarBuffer[(j*100+4)+(i*3)]), 2);
//			    memcpy(&intensity, &(lidarBuffer[(j*100+4)+(i*3+2)]), 1);
			    double R = r * 0.002;
			    double PHI = phi[i];
			    int index = getIndex(THETA,i);
			    double Rref = Rdata[index][2];
			    // 4. determine if diff(data,Rdata) passes threshold, add to list
			    if ((R != 0 && Rref != 0 && (Rref-R)/Rref >= RTHRESHOLD))
			    {
				// 5. this is where we parse the point list into pixel blocks
				double th = (2*THETA-THETA_MAX-THETA_MIN)/(THETA_MAX-THETA_MIN) + 1;
				double ph = -((PHI+10)/21) + 1;
				activePt.w = int(th*srcDims.w/2);
				activePt.h = int(ph*srcDims.h/2);
				if (activePt.w > srcDims.w || activePt.h > srcDims.h || activePt.w < 0 || activePt.h < 0)
				{
				    std::cerr << "error: converted point out of bounds" << std::endl;
				    continue;
				}
				bool activate = true;
				for (int a = 0; a < activePoints.size(); a++)
				    if (activePoints[a].w == activePt.w && activePoints[a].h == activePt.h) { activate = false; break; }
				if (activate)
				{
				    // if a newly active point, start FORGET
				    activePt.forget = 1;
				    activePoints.push_back(activePt);
				    for (int a = -HTHICK+1; a < HTHICK; a++)
				    {
					for (int b = -WTHICK+1; b < WTHICK; b++)
					{
					    H = activePt.h+a; W = activePt.w+b;
					    if (W >= srcDims.w || W < 0 || H >= srcDims.h || H < 0) continue;
					    else src1.at<cv::Vec3b>(H,W) = src2.at<cv::Vec3b>(H,W);
					}
				    }
				}
			    }
			}
		    }
		    if (previousRotationValue < THETA_MAX && THETA > THETA_MAX)
		    {
		        cv::imshow("image",src1);
		        cv::waitKey(1);
		        for (int i = 0; i < activePoints.size(); i++)
		        {
			    if (activePoints[i].forget != 0) activePoints[i].forget++; // increment + & - forget counters
			    if (activePoints[i].forget == 0) activePoints.erase(activePoints.begin()+i); // remove once cooldown complete
			    if (activePoints[i].forget == FORGET) // flip from + to - forget and deactivate pixels
			    {
				activePoints[i].forget = -FORGET;
				if (activePoints[i].w > srcDims.w || activePoints[i].h > srcDims.h || 
				    activePoints[i].w < 0 || activePoints[i].h < 0)
				{
				    std::cerr << "error: converted point out of bounds" << std::endl;
				    continue;
				}
				for (int a = -HTHICK+1; a < HTHICK; a++)
				{
				    for (int b = -WTHICK+1; b < WTHICK; b++)
				    {
					H = activePoints[i].h+a; W = activePoints[i].w+b;
					if (W >= srcDims.w || W < 0 || H >= srcDims.h || H < 0) continue;
					else src1.at<cv::Vec3b>(H,W) = src3.at<cv::Vec3b>(H,W);
				    }
				}
//				std::cout<<"FORGET at "<<Rdata[i][0]<<" "<<Rdata[i][1]<<std::endl;
			    }
		        }
		    }
		    previousRotationValue = THETA;
//		    clock_gettime(CLOCK_REALTIME,&stop);
//		    std::cout<<stop.tv_sec-start.tv_sec<<" "<<stop.tv_nsec-start.tv_nsec<<std::endl;
                    continue;
                }
		else std::cerr << "Got a partial packet" << std::endl;
            }
	else std::cerr << "Got a packet from the wrong source" << std::endl;
        };
    };
};
