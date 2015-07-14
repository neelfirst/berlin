#include <sys/types.h>
#include <dirent.h>
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
const double phi[32] = {-30.67,-9.33,-29.33,-8.00,-28.00,-6.66,-26.66,-5.33,
			-25.33,-4.00,-24.00,-2.67,-22.67,-1.33,-21.33,0.00,
			-20.00,1.33,-18.67,2.67,-17.33,4.00,-16.00,5.33,
			-14.67,6.67,-13.33,8.00,-12.00,9.33,-10.67,10.67};

const std::string DIRECTORY = "./images/";
const std::string FILENAME = "berkeley.csv"; // sync with getZero.py
const int THETA_MIN = -19; // sync with getZero.py
const int THETA_MAX = 19; // sync with getZero.py
const int WTHICK = 3; // tune per display = equals horzpixels / 12*thetarange
const int HTHICK = 15; // tune per display = equals vertpixels / 64

const int MAPTIME = 600; // time, in seconds, to linger on a map/sat pair
const int FORGET = 50; // tune for performance and aesthetics
const double RTHRESHOLD = 0.5; // tune for performance and aesthetics
//const double ITHRESHOLD = 0.2; // unused

int getIndex (double th, int phindex) { return (int(th) + (0 - THETA_MIN))*32 + phindex; }
bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length())
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    else return false;
}
int getdir (std::string dir, std::vector<std::string> &files)
{
    DIR *dp;
    std::string temp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        std::cerr << "Error(" << errno << ") opening " << dir << std::endl;
        return errno;
    }
    while ((dirp = readdir(dp)) != NULL) {
	temp = std::string(dirp->d_name);
	if (hasEnding(temp,".png"))
	        files.push_back(std::string(dirp->d_name));
    }
    closedir(dp);
    sort(files.begin(), files.end());
    return 0;
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
    const int BASEROWS = (1+THETA_MAX-THETA_MIN)*32; // expected rows in base data file
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

    // need to insert into loop to loop through sets of images
    std::vector<std::string> files = std::vector<std::string>();
    getdir(DIRECTORY,files);
    timespec start, stop;

    // Enter a loop which does a blocking read() for the next datagram
    // gets the system time as soon as that read returns
    // and then does all of the analysis.
    for ( int Z = 0; Z < files.size(); Z+=2)
    {
        std::string inMap = DIRECTORY + files[Z]; // #-map.png
        std::string inSat = DIRECTORY + files[Z+1]; // #-sat.png
        cv::Mat output = cv::imread(inMap, CV_LOAD_IMAGE_COLOR);
        cv::Mat inputSat = cv::imread(inSat, CV_LOAD_IMAGE_COLOR);
        cv::Mat inputMap = cv::imread(inMap, CV_LOAD_IMAGE_COLOR);
        srcDims.w = output.cols; srcDims.h = output.rows;
        if (srcDims.w != inputSat.cols || srcDims.h != inputSat.rows)
        {
	    std::cerr << "fatal: source image dimensional mismatch" << std::endl;
	    _exit(4);
        }
        if (Z + 2 >= files.size()) Z = -2;
        clock_gettime(CLOCK_REALTIME,&start);
        do
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
//			        memcpy(&intensity, &(lidarBuffer[(j*100+4)+(i*3+2)]), 1);
			        double R = r * 0.002;
			        double PHI = phi[i];
			        double Rref = Rdata[getIndex(THETA,i)][2];
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
					        else output.at<cv::Vec3b>(H,W) = inputSat.at<cv::Vec3b>(H,W);
					    }
				        }
				    } // if activate
			        } // if R != 0
			    } // i loop
		        } // j loop
		        if (previousRotationValue < THETA_MAX && THETA > THETA_MAX)
		        {
		            cv::imshow("image",output);
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
					    else output.at<cv::Vec3b>(H,W) = inputMap.at<cv::Vec3b>(H,W);
				        }
				    }
			        }
		            } // i loop
		        } // if previous
		        previousRotationValue = THETA;
		        clock_gettime(CLOCK_REALTIME,&stop);
                        continue;
                    } // bytesReceived > 1
		    else std::cerr << "Got a partial packet" << std::endl;
                } // ip_address
	        else std::cerr << "Got a packet from the wrong source" << std::endl;
            } // AF_INET
        } while (stop.tv_sec - start.tv_sec < MAPTIME);
    } // for
    return 0;
} // main
