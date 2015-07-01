#include <cstdlib>
#include <iostream>
#include <ctime>
#include <Magick++.h>

using namespace std;
using namespace Magick;

int getRandom(int, int);

int main(int argc, char **argv)
{
    cout << argc << endl;
    InitializeMagick(*argv);
    srand((unsigned)time(0));

    Image image1("image3.jpg");
    image1.modifyImage();
    image1.type(TrueColorType);
    Image image2("image2.jpg");
    Geometry x = image1.size();
    int w = (int)x.width();
    int h = (int)x.height();

    for (int i = 0; i < 100000; i++)
    {
	int W = getRandom(1,w);
	int H = getRandom(1,h);
	PixelPacket *pixel1 = image1.getPixels(W,H,1,1);
	PixelPacket *pixel2 = image2.getPixels(W,H,1,1);
	*pixel1 = *pixel2;
	image1.syncPixels();
//	image1.display();
	if (i%10000 == 0) image1.write("image1.jpg");
    }
    return 0;
}

int getRandom(int low, int high)
{
    if (high <= low)
    {
	cerr << "getRandom(): upper bound less than lower bound" << endl;
	exit(1);
    }
    int range = high - low;
    double rval = double(rand())/double(RAND_MAX);
    rval *= range;
    rval += low;
    return (int)rval;
}
