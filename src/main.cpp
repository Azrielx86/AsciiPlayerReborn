#include "miniaudio.h"
#include <format>
#include <getopt.h>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <thread>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

const char *chars = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
// const char* chars = " .:-=+*#%@";
const size_t charslen = strlen(chars);
volatile bool stopProgram = false;

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

void SignalHandle(const int signal)
{
	std::cout << "Received signal " << signal << "\n";
	stopProgram = true;
}

void DataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
	const auto fifo = static_cast<AVAudioFifo *>(pDevice->pUserData);
	av_audio_fifo_read(fifo, &pOutput, frameCount);
	(void) pInput;
}

void PrintFrame(char **buffer, const uint32_t width, const uint32_t height)
{
	printf("\033[H");
	fflush(stdout);
	for (size_t i = 0; i < height; i++)
	{
		if (write(1, buffer[i], width) < 0) throw std::runtime_error("Error on write() call");
		putchar('\n');
	}
	if (write(1, buffer[height - 1], width) < 0) throw std::runtime_error("Error on write() call");
}

void FrameToBuffer(const cv::Mat &img, char **buffer)
{
	for (int i = 0; i < img.rows; i++)
		for (int j = 0; j < img.cols; j++)
			buffer[i][j] = chars[((img.at<uchar>(i, j) * charslen) / 256)];
}

int PlayAudio(const std::string &filepath)
{
	AVFormatContext *format_ctx = nullptr;
	int av_ret = 0;

	if (avformat_open_input(&format_ctx, filepath.c_str(), nullptr, nullptr) < 0)
	{
		std::cerr << "Cannot open media!\n";
		return -1;
	}

	if (avformat_find_stream_info(format_ctx, nullptr) < 0)
	{
		std::cerr << "Cannot find stream info\n";
		return -1;
	}

	int index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (index < 0)
	{
		std::cerr << "No audio found in media!\n";
		return -1;
	}

	AVStream *stream = format_ctx->streams[index];
	const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!decoder)
	{
		std::cerr << "No decoder found for file!\n";
		return -1;
	}

	AVCodecContext *codec_context = avcodec_alloc_context3(decoder);
	if (!codec_context)
	{
		std::cerr << "Could not allocate audio codec context.\n";
		return -1;
	}
	avcodec_parameters_to_context(codec_context, stream->codecpar);

	if (avcodec_open2(codec_context, decoder, nullptr) < 0)
	{
		std::cerr << "Could not open decoder!\n";
		return -1;
	}

	// ===== Decode audio
	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	SwrContext *resampler = nullptr;
	swr_alloc_set_opts2(&resampler, &stream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, stream->codecpar->sample_rate,
	                    &stream->codecpar->ch_layout, static_cast<AVSampleFormat>(stream->codecpar->format), stream->codecpar->sample_rate, 00, nullptr);
	AVAudioFifo *fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, stream->codecpar->ch_layout.nb_channels, 1);

	while (!av_read_frame(format_ctx, packet))
	{
		if (packet->stream_index != index) continue;

		av_ret = avcodec_send_packet(codec_context, packet);
		if (av_ret < 0)
			if (av_ret != AVERROR(EAGAIN))
				std::cerr << "Error while decoding!\n";

		while (avcodec_receive_frame(codec_context, frame) == 0)
		{
			AVFrame *resampled_frame = av_frame_alloc();
			resampled_frame->sample_rate = frame->sample_rate;
			resampled_frame->ch_layout = frame->ch_layout;
			resampled_frame->format = AV_SAMPLE_FMT_FLT;

			swr_convert_frame(resampler, resampled_frame, frame);
			av_frame_unref(frame);

			av_audio_fifo_write(fifo, reinterpret_cast<void **>(resampled_frame->data), resampled_frame->nb_samples);
			av_frame_free(&resampled_frame);
		}
	}

	// ===== Audio playback
	ma_device_config device_config;
	ma_device device;

	device_config = ma_device_config_init(ma_device_type_playback);
	device_config.playback.format = ma_format_f32;
	device_config.playback.channels = stream->codecpar->ch_layout.nb_channels;
	device_config.sampleRate = stream->codecpar->sample_rate;
	device_config.dataCallback = &DataCallback;
	device_config.pUserData = fifo;

	avformat_close_input(&format_ctx);
	av_frame_free(&frame);
	av_packet_free(&packet);
	avcodec_free_context(&codec_context);
	swr_free(&resampler);

	if (ma_device_init(nullptr, &device_config, &device) != MA_SUCCESS)
	{
		std::cerr << "Failed to open playback device!\n";
		return -1;
	}

	if (ma_device_start(&device) != MA_SUCCESS)
	{
		std::cerr << "Failed to start playback device!\n";
		ma_device_uninit(&device);
		return -1;
	}

	while (av_audio_fifo_size(fifo) && !stopProgram)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	ma_device_uninit(&device);
	av_audio_fifo_free(fifo);
	return 0;
}

int main(const int argc, char **argv)
{
	uint32_t width = 130;
	uint32_t height = 30;
	std::string filepath;

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

	signal(SIGTERM, SignalHandle);
	signal(SIGINT, SignalHandle);

	cv::utils::logging::setLogLevel(cv::utils::logging::LogLevel::LOG_LEVEL_SILENT);

	auto audio_thread = std::thread(&PlayAudio, filepath);

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

	printf("\033[2J\033[H");
	fflush(stdout);
	while (cap.isOpened() && !stopProgram)
	{
		if (!cap.read(frame)) break;

		cv::cvtColor(frame, grayscale, cv::COLOR_BGR2GRAY);
		cv::resize(grayscale, final, cv::Size(static_cast<int>(width), static_cast<int>(height)));

		FrameToBuffer(final, buffer);
		PrintFrame(buffer, width, height);

		timer.Sleep();
	}

	if (cap.isOpened())
		cap.release();

	if (audio_thread.joinable())
		audio_thread.join();

	for (size_t i = 0; i < height; i++)
		delete[] buffer[i];
	delete[] buffer;

	printf("\033[2J\033[H^-^!\n");
	fflush(stdout);

	return 0;
}
