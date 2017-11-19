#pragma once

/*
* FFmpegXX
* Copyright (C) 2017 oleksa
*
* C++ wrapper of ffmpeg library https://ffmpeg.org
*
* FFmpegXX is free software; you can redistribute it and/or
* modify it under the terms of the MIT License
*
*/

extern "C"
{
#include <libavcodec/version.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <exception>
#include <iostream>
#include <fstream>

namespace ffmpegxx {

	static_assert(LIBAVFORMAT_BUILD >= AV_VERSION_INT(57, 0, 0), "ffmpeg version is old");

	class CAcDictionary
	{
	public:

		CAcDictionary()
		{
		}

		~CAcDictionary()
		{
			av_dict_free(&m_dict);
		}

		CAcDictionary(CAcDictionary const & b) = delete;

		CAcDictionary(CAcDictionary && b)
		{
			m_dict = b.m_dict;
			b.m_dict = nullptr;
		}

		operator AVDictionary **()
		{
			return &m_dict;
		}

		int set(const char *key, const char *value, int flags = 0)
		{
			return av_dict_set(&m_dict, key, value, 0);
		}

	protected:

		AVDictionary * m_dict = nullptr;
	};

	class CAvPacket
	{
	public:

		CAvPacket()
		{
			av_init_packet(&m_packet);
		}

		~CAvPacket()
		{
			av_packet_unref(&m_packet);
		}

		CAvPacket(CAvPacket const & b) = delete;
		CAvPacket(CAvPacket && b) = delete;

		void reset()
		{
			av_packet_unref(&m_packet);
		}

		AVPacket * get()
		{
			return &m_packet;
		}

		AVPacket const * get() const
		{
			return &m_packet;
		}

		uint8_t const * data() const
		{
			return m_packet.data;
		}

		size_t size() const
		{
			return m_packet.size;
		}

	private:

		AVPacket m_packet;
	};

	class CAvFrame
	{
	public:

		CAvFrame()
			: m_frame(av_frame_alloc())
		{
		}

		CAvFrame(AVPixelFormat format, int width, int height, int align = 32)
			: m_frame(av_frame_alloc())
		{
			m_frame->format = format;
			m_frame->width = width;
			m_frame->height = height;

			if (av_frame_get_buffer(m_frame, align) != 0)
			{
				av_free(m_frame);
				throw std::bad_alloc();
			}
		}

		~CAvFrame()
		{
			av_frame_free(&m_frame);
		}

		CAvFrame(CAvFrame const & b) = delete;

		CAvFrame(CAvFrame && b)
		{
			m_frame = b.m_frame;
			b.m_frame = nullptr;
		}

		CAvFrame & operator = (CAvFrame && b)
		{
			if (this != &b)
			{
				m_frame = b.m_frame;
				b.m_frame = nullptr;
			}
			return *this;
		}

		AVFrame * get()
		{
			return m_frame;
		}

		AVFrame const * get() const
		{
			return m_frame;
		}

		void reset(AVPixelFormat format, int width, int height, int align = 32)
		{
			av_frame_unref(m_frame);

			m_frame->format = format;
			m_frame->width = width;
			m_frame->height = height;

			if (av_frame_get_buffer(m_frame, align) != 0)
				throw std::bad_alloc();
		}

		AVPixelFormat format() const
		{ 
			return AVPixelFormat(m_frame->format); 
		}

		int width() const 
		{ 
			return m_frame->width; 
		}
		
		int height() const	
		{ 
			return m_frame->height; 
		}

	protected:

		AVFrame * m_frame = nullptr;
	};

	class CAvCodecContext
	{
	public:

		CAvCodecContext()
		{
			m_ctx = avcodec_alloc_context3(NULL);
			if (!m_ctx) throw std::bad_alloc();
		}

		CAvCodecContext(AVCodec const * codec)
		{
			m_ctx = avcodec_alloc_context3(codec);
			if (!m_ctx) throw std::bad_alloc();
		}

		~CAvCodecContext()
		{
			avcodec_free_context(&m_ctx);
		}

		CAvCodecContext(CAvCodecContext const &) = delete;
		CAvCodecContext(CAvCodecContext &&) = delete;

		int set_parameters(AVCodecParameters const * src_par)
		{
			return avcodec_parameters_to_context(m_ctx, src_par);
		}

		AVCodecContext * get()
		{
			return m_ctx;
		}

		AVCodecContext const * get() const
		{
			return m_ctx;
		}

		void reset(AVCodec const * codec)
		{
			avcodec_free_context(&m_ctx);

			m_ctx = avcodec_alloc_context3(codec);
			if (!m_ctx) throw std::bad_alloc();
		}

		int open(AVCodec * codec = nullptr)
		{
			int res = avcodec_open2(m_ctx, codec, NULL);
			if (res < 0)
				return res;

			return res;
		}

		int open(AVCodecParameters const *parameters)
		{
			int res;

			AVCodec * codec = nullptr;

			if (!m_ctx->codec || m_ctx->codec_id != parameters->codec_id)
			{
				codec = avcodec_find_decoder(parameters->codec_id);
				if (!codec)
				{
					// codec not registered
					throw std::logic_error("avcodec not registered");
				}

				reset(codec);
			}

			res = set_parameters(parameters);
			if (res < 0)
				return res;

			res = avcodec_open2(m_ctx, codec, NULL);
			if (res < 0)
				return res;

			return res;
		}

	protected:

		AVCodecContext * m_ctx;
	};


	class CAvFormatContext
	{
	public:

		CAvFormatContext()
		{
			m_ctx = avformat_alloc_context();
		}

		~CAvFormatContext()
		{
			avformat_free_context(m_ctx);
		}

		CAvFormatContext(CAvFormatContext const &) = delete;
		CAvFormatContext(CAvFormatContext &&) = delete;

		AVFormatContext * get()
		{
			return m_ctx;
		}

		AVFormatContext const * get() const
		{
			return m_ctx;
		}

		bool is_video_packet(CAvPacket const & packet)
		{
			unsigned int index = packet.get()->stream_index;
			return index < m_ctx->nb_streams &&
				m_ctx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
		}

		bool is_audio_packet(CAvPacket const & packet)
		{
			unsigned int index = packet.get()->stream_index;
			return index < m_ctx->nb_streams &&
				m_ctx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
		}

	protected:

		AVFormatContext * m_ctx;
	};

	class CAvStreamReader : public CAvFormatContext
	{
	public:

		CAvStreamReader()
			: CAvFormatContext()
		{
		}

		int open(const char * url)
		{
			int res;

			res = avformat_open_input(&m_ctx, url, NULL, m_opts);
			if (res < 0)
				return res;

			res = avformat_find_stream_info(m_ctx, NULL);
			if (res < 0)
				return res;

			m_video_stream_index = -1;
			m_audio_stream_index = -1;

			for (unsigned int i = 0; i < m_ctx->nb_streams; i++)
			{
				if (m_video_stream_index < 0 && m_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					m_video_stream_index = i;
				}

				if (m_audio_stream_index < 0 && m_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
				{
					m_audio_stream_index = i;
				}
			}

			return res;
		}

		int play()
		{
			return av_read_play(m_ctx);
		}

		int pause()
		{
			return av_read_pause(m_ctx);
		}

		AVCodecParameters const * get_codecpar(AVMediaType codec_type)
		{
			for (unsigned int i = 0; i < m_ctx->nb_streams; i++)
			{
				auto * par = m_ctx->streams[i]->codecpar;
				if (par->codec_type == codec_type)
					return par;
			}
			return nullptr;
		}

		AVCodecParameters const * get_video_codecpar()
		{
			if (m_video_stream_index >= 0)
				return m_ctx->streams[m_video_stream_index]->codecpar;
			else
				return nullptr;
		}

		int read_frame(CAvPacket & packet)
		{
			return av_read_frame(m_ctx, packet.get());
		}

		int get_video_stream_index()
		{
			return m_video_stream_index;
		}

		CAcDictionary & options()
		{
			return m_opts;
		}

		int set_option(const char * key, const char * value)
		{
			return m_opts.set(key, value);
		}

	protected:

		int m_video_stream_index = -1;
		int m_audio_stream_index = -1;

		CAcDictionary m_opts;
	};

	class CAvDecoder : public CAvCodecContext
	{
	public:

		CAvDecoder()
			: CAvCodecContext()
		{
		}

		CAvDecoder(AVCodec * codec)
			: CAvCodecContext(codec)
		{
		}

		int decode(CAvPacket const & packet)
		{
			return avcodec_send_packet(m_ctx, packet.get());
		}

		int decode_end()
		{
			return avcodec_send_packet(m_ctx, NULL);
		}

		int receive_frame(CAvFrame & frame)
		{
			// returns: 0 - ok, AVERROR_EOF
			return avcodec_receive_frame(m_ctx, frame.get());
		}

	protected:
	};

	class CAvEncoder : public CAvCodecContext
	{
	public:

		CAvEncoder()
			: CAvCodecContext()
		{
		}

		CAvEncoder(AVCodec * codec)
			: CAvCodecContext(codec)
		{
		}

		int encode(CAvFrame const & frame)
		{
			return avcodec_send_frame(m_ctx, frame.get());
		}

		int encode_end()
		{
			return avcodec_send_frame(m_ctx, NULL);
		}

		int receive_packet(CAvPacket & packet)
		{
			return avcodec_receive_packet(m_ctx, packet.get());
		}

	protected:
	};

	class CSwsContext
	{
	public:

		CSwsContext()
		{
		}

		~CSwsContext()
		{
			sws_freeContext(m_ctx);
		}

		CSwsContext(CSwsContext const &b) = delete;
		CSwsContext(CSwsContext &&b)
		{
			m_ctx = b.m_ctx;
			b.m_ctx = nullptr;
		}

		int convert(CAvFrame const & input_picture, CAvFrame & output_picture)
		{
			if (input_picture.format() == AVPixelFormat::AV_PIX_FMT_NONE ||
				output_picture.format() == AVPixelFormat::AV_PIX_FMT_NONE)
			{
				throw std::invalid_argument(__FUNCTION__);
			}

			m_ctx = sws_getCachedContext(
				m_ctx,
				input_picture.width(), input_picture.height(),
				input_picture.format(),
				output_picture.width(), output_picture.height(),
				output_picture.format(),
				SWS_BICUBIC,
				NULL, NULL, NULL
			);

			int res;

			res = sws_scale(
				m_ctx,
				input_picture.get()->data,
				input_picture.get()->linesize,
				0, input_picture.height(),
				output_picture.get()->data,
				output_picture.get()->linesize
			);

			return res;
		}

		int convert(CAvFrame const & input_picture, AVPixelFormat output_pix_fmt, CAvFrame & output_picture)
		{
			output_picture.reset(output_pix_fmt, input_picture.width(), input_picture.height());

			return convert(input_picture, output_picture);
		}

	protected:

		SwsContext * m_ctx = nullptr;
	};


} // namespace ffmpegxx


