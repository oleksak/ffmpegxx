
#include "ffmpegxx.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

#include <stdexcept>

using namespace ffmpegxx;

struct av_error
{
	av_error(int err) : m_error(err) {}

	friend std::ostream & operator << (std::ostream &s, av_error const & v)
	{
		switch (v.m_error)
		{
		case AVERROR(EINVAL):			s << "EINVAL"; break;
		case AVERROR(EADDRNOTAVAIL):	s << "EADDRNOTAVAIL"; break;
		case AVERROR(ETIMEDOUT):		s << "ETIMEDOUT"; break;
		case AVERROR(EAGAIN):			s << "EAGAIN"; break;
		case AVERROR_INVALIDDATA:		s << "AVERROR_INVALIDDATA"; break;
		case AVERROR_EOF:				s << "AVERROR_EOF"; break;
		default:
			if (AVUNERROR(v.m_error) < 0x01000000)
				s << AVUNERROR(v.m_error);
			else
				s << std::hex << AVUNERROR(v.m_error);
		}

		return s;
	}

	int m_error;
};

void log_callback(void *avcl, int level, const char *fmt, va_list vargs)
{
	static char message[8192];
	const char *module_name = NULL;

	if (avcl)
	{
		AVClass *avc = *(AVClass**)avcl;
		module_name = avc->item_name(avcl);
	}

	vsnprintf(message, sizeof(message), fmt, vargs);

	std::cout << "(" << level << ")";

	if (module_name)
		std::cout << "[" << module_name << "] LOG: " << message << std::endl;
	else
		std::cout << "[*] LOG: " << message << std::endl;
}

bool save_image_to_file(CAvFrame const & picture, const char * fname, AVCodecID codec_id)
{
	AVCodec *codec = avcodec_find_encoder(codec_id);
	if (!codec)
		return false;

	CAvEncoder encoder(codec);

	encoder.get()->time_base = av_make_q(1, 1);
	encoder.get()->pix_fmt = AVPixelFormat(picture.get()->format);
	encoder.get()->height = picture.get()->height;
	encoder.get()->width = picture.get()->width;
	encoder.get()->sample_aspect_ratio = picture.get()->sample_aspect_ratio;
	encoder.get()->compression_level = 100;
	//encoder.get()->thread_count = 1;
	//encoder.get()->prediction_method = 1;
	//encoder.get()->flags2 = 0;
	//encoder.get()->rc_max_rate = encoder.get()->rc_min_rate = encoder.get()->bit_rate = 80000000;

	int res;

	res = encoder.open();
	if (res < 0)
	{
		std::cout << "encoder.open: error: " << av_error(res) << std::endl;
		return false;
	}

	res = encoder.encode(picture);
	if (res < 0)
	{
		std::cout << "encoder.encode: error: " << av_error(res) << std::endl;
		return false;
	}

	CAvPacket packet;
	res = encoder.receive_packet(packet);
	if (res < 0)
	{
		std::cout << "encoder.receive_packet: error: " << av_error(res) << std::endl;
		return false;
	}

	std::ofstream output_file;
	output_file.open(fname, std::ios::binary);

	if (!output_file)
		return false;

	output_file.write((const char*)packet.data(), packet.size());

	output_file.close();
	return true;
}

bool save_as_jpg(CAvFrame const & picture, const char * fname)
{
	CAvFrame picture2;
	CSwsContext scaler;
	scaler.convert(picture, AV_PIX_FMT_YUVJ422P, picture2);

	return save_image_to_file(picture2, fname, AV_CODEC_ID_MJPEG);
}

bool save_as_png(CAvFrame const & picture, const char * fname)
{
	CAvFrame picture2;
	CSwsContext scaler;
	scaler.convert(picture, AV_PIX_FMT_RGB24, picture2);

	return save_image_to_file(picture2, fname, AV_CODEC_ID_PNG);
}

bool save_as_ppm(CAvFrame const & picture, const char * fname)
{
	CAvFrame picture2;
	CSwsContext scaler;
	scaler.convert(picture, AV_PIX_FMT_RGB24, picture2);

	return save_image_to_file(picture2, fname, AV_CODEC_ID_PPM);
}

class CAvRtspCapturer
{
public:

	CAvRtspCapturer()
	{
	}

	~CAvRtspCapturer()
	{
	}

	bool open(const char * url)
	{
		// see: https://ffmpeg.org/ffmpeg-protocols.html#rtsp

#if 0
		reader.set_option("rtsp_transport", "udp");

		//reader.set_option("timeout", "5M");

		//reader.set_option("analyzeduration", "10M");
		//reader.set_option("probesize", "10M");

		//reader.set_option("rtsp_flags", "filter_src");

#else
		reader.set_option("rtsp_transport", "tcp");

		//reader.set_option("analyzeduration", "10M");
		//reader.set_option("probesize", "10M");

#endif

		reader.set_option("stimeout", "10M");

		//reader.set_option("rtsp_flags", "filter_src");
		//reader.set_option("rtsp_flags", "prefer_tcp");

		reader.set_option("allowed_media_types", "video");

		int res;

		res = reader.open(url);
		if (res < 0)
		{
			std::cout << "reader.open: failed: " << av_error(res) << std::endl;
			return false;
		}

		auto * video_parameters = reader.get_video_codecpar();
		if (!video_parameters)
			return false;

		std::cout << "video frame: " << video_parameters->width << " x " << video_parameters->height << std::endl;

		res = decoder.open(video_parameters);
		if (res < 0)
		{
			std::cout << "decoder.open: failed: " << av_error(res) << std::endl;
			return false;
		}

		return true;
	}

	void run(int max_frame_count)
	{
		reader.play();

		bool bGrabStop = false;

		m_frame_count = 0;

		CAvFrame picture;

		while (m_frame_count < max_frame_count)
		{
			int res;

			if (!bGrabStop && ! grab())
				bGrabStop = true;

			res = retrieve(picture);

			if (res == AVERROR(EAGAIN))
				continue;

			else if (res < 0)
				break;

			std::cout << "2 Video " << m_frame_count << std::endl;
			process_frame(picture);

			++m_frame_count;
		}

		reader.pause();
	}

	void process_frame(CAvFrame const & frame)
	{
		// save picture to file

		std::stringstream file_name;

		//file_name << "test" << m_frame_count << ".ppm";
		//save_as_ppm(frame, file_name.str().c_str());

		file_name << "test" << m_frame_count << ".jpg";
		save_as_jpg(frame, file_name.str().c_str());

		//file_name << "test" << m_frame_count << ".png";
		//save_as_png(frame, file_name.str().c_str());
	}

protected:

	bool grab()
	{
		CAvPacket packet;

		int res = reader.read_frame(packet);

		if (res == AVERROR(EAGAIN))
		{
			// continue;
			return true;
		}
		else if (res == AVERROR_EOF)
		{
			decoder.decode_end();
			return false;
		}
		else if (res < 0)
		{
			// error
			std::cout << "reader.read_frame: error: " << av_error(res) << std::endl;

			decoder.decode_end();
			return false;
		}

		if (reader.is_video_packet(packet))
		{
			res = decoder.decode(packet);
			if (res < 0)
			{
				std::cout << "decoder.decode: error: " << av_error(res) << std::endl;

				decoder.decode_end();
				return false;
			}
		}

		return true;
	}

	int retrieve(CAvFrame & picture)
	{
		int res = decoder.receive_frame(picture);
		if (res < 0)
		{
			std::cout << "decoder.receive_frame: error: " << av_error(res) << std::endl;
			return res;
		}

		return res;
	}

protected:

	CAvStreamReader reader;
	CAvDecoder decoder;

	int m_frame_count;
};

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "using: read_rtsp <rtsp_url>" << std::endl;
		return EXIT_FAILURE;
	}

	const char * url = argv[1];

	std::cout << "opening \'" << url << "\'" << std::endl;

	avformat_network_init();
	av_register_all();

	//av_log_set_level(AV_LOG_TRACE);
	av_log_set_level(AV_LOG_ERROR);
	//av_log_set_callback(&log_callback);

	CAvRtspCapturer reader;

	try
	{
		if (!reader.open(url))
			return EXIT_FAILURE;

		reader.run(1000);
	}
	catch (std::exception const & e)
	{
		std::cout << "[EX] " << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
