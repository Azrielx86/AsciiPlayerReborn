#include <bits/this_thread_sleep.h>
#include <format>
#include <getopt.h>
#include <iostream>
#include <opencv2/opencv.hpp>

const char *chars = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
// const char* chars = " .:-=+*#%@";
const size_t charslen = strlen(chars);

static option long_options[] = {
    {"width", optional_argument, nullptr, 'w'},
    {"height", optional_argument, nullptr, 'h'},
    {"file", required_argument, nullptr, 'f'},
    {nullptr, 0, nullptr, 0}};

class Timer
{
	double frame_duration;
	std::chrono::time_point<std::chrono::steady_clock> last_call_end_time;
	std::chrono::time_point<std::chrono::steady_clock> last_zero_call_end_time;
	int last_zero_count;

  public:
	explicit Timer(const double fps)
	{
		frame_duration = 1.0 / fps;
		last_call_end_time = std::chrono::steady_clock::now();
		last_zero_call_end_time = last_call_end_time;
		last_zero_count = 1;
	}

	std::chrono::duration<float, std::chrono::steady_clock::period> Sleep()
	{
		const auto sleep_time = last_zero_call_end_time + std::chrono::duration<double>(frame_duration * static_cast<double>(last_zero_count)) - std::chrono::steady_clock::now();

		if (sleep_time < std::chrono::duration<float>(0))
		{
			last_call_end_time = std::chrono::steady_clock::now();
			last_zero_call_end_time = last_call_end_time;
			last_zero_count = 1;
			return std::chrono::duration<float>(0);
		}

		std::this_thread::sleep_for(sleep_time);

		last_call_end_time = std::chrono::steady_clock::now();
		last_zero_count += 1;
		return sleep_time;
	}
};

void PrintFrame(char **buffer, const uint32_t width, const uint32_t height)
{
	for (size_t i = 0; i < height; i++)
	{
		if (write(1, buffer[i], width) < 0) throw std::runtime_error("Error on write() call");
		putchar('\n');
	}
}

void FrameToBuffer(const cv::Mat &img, char **buffer)
{
	for (int i = 0; i < img.rows; i++)
		for (int j = 0; j < img.cols; j++)
			buffer[i][j] = chars[((img.at<uchar>(i, j) * charslen) / 256)];
}

int main(const int argc, char **argv)
{
	uint32_t width = 130;
	uint32_t height = 30;
	std::string filepath = "../test.png";

	int opt;
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, "w:h:f:", long_options, &option_index)) != -1)
	{
		switch (opt)
		{
		case 'w':
			width = std::stoi(optarg);
			break;
		case 'h':
			height = std::stoi(optarg);
			break;
		case 'f':
			filepath = std::string(optarg);
		default:;
		}
	}

	std::cout << std::format("Using video: {}\n", filepath);

	const auto buffer = new char *[height];
	for (size_t i = 0; i < height; i++)
		buffer[i] = new char[width];

	cv::Mat frame, grayscale, final;
	auto cap = cv::VideoCapture(filepath);

	if (!cap.isOpened())
		throw std::runtime_error("Cannot open file!\n");

	const auto fps = cap.get(cv::CAP_PROP_FPS);

	Timer timer(fps);

	while (cap.isOpened())
	{
		if (!cap.read(frame)) break;

		cv::cvtColor(frame, grayscale, cv::COLOR_BGR2GRAY);
		cv::resize(grayscale, final, cv::Size(static_cast<int>(width), static_cast<int>(height)));

		FrameToBuffer(final, buffer);
		printf("\033[2J\033[H");
		PrintFrame(buffer, width, height);

		timer.Sleep();
	}

	for (size_t i = 0; i < height; i++)
		delete[] buffer[i];
	delete[] buffer;

	return 0;
}
