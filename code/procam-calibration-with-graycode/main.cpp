#include <iostream>
#include <vector>
#include <sstream>

#include <opencv2/opencv.hpp>  // checked at opencv 3.4.1
#include <opencv2/structured_light.hpp>

#define WINDOWWIDTH 1920
#define WINDOWHEIGHT 1080
#define GRAYCODEWIDTHSTEP 5
#define GRAYCODEHEIGHTSTEP 5
#define GRAYCODEWIDTH WINDOWWIDTH / GRAYCODEWIDTHSTEP
#define GRAYCODEHEIGHT WINDOWHEIGHT / GRAYCODEHEIGHTSTEP
#define WHITETHRESHOLD 5
#define BLACKTHRESHOLD 40

cv::VideoCapture camera;

// �������g���J�����ɍ��킹�Ď�������
void initializeCamera() 
{
	camera = cv::VideoCapture(0);
}
cv::Mat getCameraImage() 
{
	// Capture a frame from ccamera
	cv::Mat image;
	camera.read(image);
	return image.clone();
}
void terminateCamera() {}

// Camera to Projector
struct C2P {
	int cx;
	int cy;
	int px;
	int py;
	C2P(int camera_x, int camera_y, int proj_x, int proj_y) 
	{
		cx = camera_x;
		cy = camera_y;
		px = proj_x;
		py = proj_y;
	}
};

int main() 
{
	// -----------------------------------
	// ----- Prepare graycode images -----
	// -----------------------------------
	cv::structured_light::GrayCodePattern::Params params;
	params.width = GRAYCODEWIDTH;
	params.height = GRAYCODEHEIGHT;
	auto pattern = cv::structured_light::GrayCodePattern::create(params);

	// �p�r:decode����positive��negative�̉�f�l�̍���
	//      ���whiteThreshold�ȏ�ł����f�̂�decode����
	pattern->setWhiteThreshold(WHITETHRESHOLD);
	// �p�r:ShadowMask�v�Z���� white - black > blackThreshold
	//      �Ȃ�ΑO�i�i�O���C�R�[�h��F�������j�Ɣ��ʂ���
	// ����͂����ݒ肵�Ă��Q�Ƃ���邱�Ƃ͂Ȃ����ꉞ�Z�b�g���Ă���
	pattern->setBlackThreshold(BLACKTHRESHOLD);

	std::vector<cv::Mat> graycodes;
	pattern->generate(graycodes);

	cv::Mat blackCode, whiteCode;
	pattern->getImagesForShadowMasks(blackCode, whiteCode);
	graycodes.push_back(blackCode), graycodes.push_back(whiteCode);

	// -----------------------------
	// ----- Prepare cv window -----
	// -----------------------------
	cv::namedWindow("Pattern", cv::WINDOW_NORMAL);
	cv::resizeWindow("Pattern", GRAYCODEWIDTH, GRAYCODEHEIGHT);
	// 2���ڂ̃f�B�X�v���C�Ƀt���X�N���[���\��
	cv::moveWindow("Pattern", 1920, 0);
	cv::setWindowProperty("Pattern", cv::WND_PROP_FULLSCREEN,
		cv::WINDOW_FULLSCREEN);

	// ----------------------------------
	// ----- Wait camera adjustment -----
	// ----------------------------------
	initializeCamera();

	cv::imshow("Pattern", graycodes[graycodes.size() - 3]);
	while (true) {
		cv::Mat img = getCameraImage();
		cv::imshow("camera", img);
		if (cv::waitKey(1) != -1) break;
	}

	// --------------------------------
	// ----- Capture the graycode -----
	// --------------------------------
	std::vector<cv::Mat> captured;
	int cnt = 0;
	for (auto gimg : graycodes) {
		cv::imshow("Pattern", gimg);
		// �f�B�X�v���C�ɕ\��->�J�����o�b�t�@�ɔ��f�����܂ő҂�
		// �K�v�ȑ҂����Ԃ͎g���J�����Ɉˑ�
		cv::waitKey(400);

		cv::Mat img = getCameraImage();
		std::ostringstream oss;
		oss << std::setfill('0') << std::setw(2) << cnt++;
		cv::imwrite("cam_" + oss.str() + ".png", img);

		captured.push_back(img);
	}

	terminateCamera();

	// -------------------------------
	// ----- Decode the graycode -----
	// -------------------------------
	// pattern->decode()�͎����}�b�v�̉�͂Ɏg���֐��Ȃ̂ō���͎g��Ȃ�
	// pattern->getProjPixel()���g���Ċe�J������f�Ɏʂ����v���W�F�N�^��f�̍��W���v�Z
	cv::Mat white = captured.back();
	captured.pop_back();
	cv::Mat black = captured.back();
	captured.pop_back();

	int camHeight = captured[0].rows;
	int camWidth = captured[0].cols;

	cv::Mat c2pX = cv::Mat::zeros(camHeight, camWidth, CV_16U);
	cv::Mat c2pY = cv::Mat::zeros(camHeight, camWidth, CV_16U);
	std::vector<C2P> c2pList;
	for (int y = 0; y < camHeight; y++) {
		for (int x = 0; x < camWidth; x++) {
			cv::Point pixel;
			if (white.at<cv::uint8_t>(y, x) - black.at<cv::uint8_t>(y, x) >
				BLACKTHRESHOLD &&
				!pattern->getProjPixel(captured, x, y, pixel)) {
				c2pX.at<cv::uint16_t>(y, x) = pixel.x;
				c2pY.at<cv::uint16_t>(y, x) = pixel.y;
				c2pList.push_back(C2P(x, y, pixel.x * GRAYCODEWIDTHSTEP,
					pixel.y * GRAYCODEHEIGHTSTEP));
			}
		}
	}

	// ---------------------------
	// ----- Save C2P as csv -----
	// ---------------------------
	std::ofstream os("c2p.csv");
	for (auto elem : c2pList) {
		os << elem.cx << ", " << elem.cy << ", " << elem.px << ", " << elem.py
			<< std::endl;
	}
	os.close();

	// ----------------------------
	// ----- Visualize result -----
	// ----------------------------
	cv::Mat viz = cv::Mat::zeros(camHeight, camWidth, CV_8UC3);
	for (int y = 0; y < camHeight; y++) {
		for (int x = 0; x < camWidth; x++) {
			viz.at<cv::Vec3b>(y, x)[0] = (unsigned char)c2pX.at<cv::uint16_t>(y, x);
			viz.at<cv::Vec3b>(y, x)[1] = (unsigned char)c2pY.at<cv::uint16_t>(y, x);
		}
	}
	cv::imshow("result", viz);
	cv::waitKey(0);

	return 0;
}