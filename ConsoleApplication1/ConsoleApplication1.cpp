// ConsoleApplication1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <codecvt>
#include <locale>
#include<filesystem>
#include "AVDemuxer.h"
#include "PipeDef.h"
#include <fstream>
#include <time.h>


BOOL                    m_stop{ FALSE };
AudioParameter          g_ap1;
AudioParameter          g_ap2;
IPCDemo::AVDemuxer*     g_demuex1{ NULL };
IPCDemo::AVDemuxer*     g_demuex2{ NULL };
std::thread             g_task1;
std::thread             g_task2;
IPipeClient*			g_client{ NULL };
std::ofstream            g_logFile;

std::wstring to_wide_string(const std::string& input)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(input);
}

BOOL LogMessageCallbackHandler(LogSeverity logLevel, LPCSTR szFile, INT32 line, LPCSTR szText)
{
    switch (logLevel)
    {
    case IPC_LOG_VERBOSE:
    case IPC_LOG_INFO:
    case IPC_LOG_WARNING:
    case IPC_LOG_ERROR:
    case IPC_LOG_FATAL:
    case IPC_LOG_NUM_SEVERITIES:
        if (g_logFile.is_open())
        {
            g_logFile << std::string(szText) << std::endl;
        }
        break;
    default:
        break;
    }

    return TRUE;
}

void OnMessagePump1()
{
	g_demuex1 = new IPCDemo::AVDemuxer();
	g_demuex1->AUDIO_START = [](const AudioParameter& ap)->void {
		g_ap1 = ap;
		};
	g_demuex1->AUDIO_PACKET = [](PipeSDK::IPC_PACKET& pkt)->void {
		pkt.channel = 0;
		if (g_client != NULL)
			g_client->WritePacket(&pkt, 1000);

		};
	g_demuex1->VIDEO_PACKET = [](PipeSDK::IPC_PACKET& pkt)->void {
		pkt.channel = 0;
		if (g_client != NULL)
			g_client->WritePacket(&pkt, 1000);
		};
	std::filesystem::path path("D:\\西虹市首富.mp4");
	g_demuex1->Open(path.u8string());
	for (;;)
	{
		if (m_stop)
			break;
		Sleep(1);
	}
	g_demuex1->Close();
	delete g_demuex1;
}

void OnMessagePump2()
{
	g_demuex2 = new IPCDemo::AVDemuxer();
	g_demuex2->AUDIO_START = [](const AudioParameter& ap)->void {
		g_ap2 = ap;
		};
	g_demuex2->AUDIO_PACKET = [](PipeSDK::IPC_PACKET& pkt)->void {
		pkt.channel = 1;
		if (g_client != NULL)
			g_client->WritePacket(&pkt, 1000);

		};
	g_demuex2->VIDEO_PACKET = [](PipeSDK::IPC_PACKET& pkt)->void {
		pkt.channel = 1;
		if (g_client != NULL)
			g_client->WritePacket(&pkt, 1000);
		};
	std::filesystem::path path("D:\\孤注一掷.mp4");
	g_demuex2->Open(path.u8string());
	for (;;)
	{
		if (m_stop)
			break;
		Sleep(1);
	}
	g_demuex2->Close();
	delete g_demuex2;
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cout << "input params invalid!" << std::endl;
        return -1;
    }
    std::string firstArg(argv[1]);
    std::string frontStr = "--pipeName=";
    std::string pipeName = firstArg.substr(frontStr.length());
    std::string secondArg(argv[2]);
    std::string frontStr2 = "--maxChannels=";
    UINT32 maxChannels = std::stoi(secondArg.substr(frontStr2.length()));

	CreatePipeClient(&g_client);
    std::time_t t = time(nullptr);
    char   filetime[256];
    strftime(filetime, sizeof(filetime), "%Y-%m-%d-%H-%M-%S", localtime(&t));
    std::string filename = std::string("client_").append(filetime).append(std::string(".log"));
    g_logFile.open(filename.c_str());
    g_client->SetLogMessageCallback(LogMessageCallbackHandler);
	g_client->SetCallback([](IPC_EVENT_TYPE type, UINT32 msg, const CHAR* data, UINT32 size, void* args) -> void {
		IPipeClient* pThis = static_cast<IPipeClient*>(args);
		if (pThis != NULL)
		{
			switch (type)
			{
			case PipeSDK::EVENT_CONNECTED:
			{
				std::cout << std::string("EVENT_CONNECTED") << std::endl;
				g_task1 = std::thread(OnMessagePump1);
				g_task2 = std::thread(OnMessagePump2);
			}
			break;
			case PipeSDK::EVENT_BROKEN:
				std::cout << std::string("EVENT_BROKEN") << std::endl;
				break;
			case PipeSDK::EVENT_DISCONNECTED:
				std::cout << std::string("EVENT_DISCONNECTED") << std::endl;
				break;
			case PipeSDK::EVENT_CONNECTION_RESET:
				std::cout << std::string("EVENT_CONNECTION_RESET") << std::endl;
				break;
			case PipeSDK::EVENT_MESSAGE:
			{
				std::string szText(data, size);
				std::cout << "Receive Msg: " << szText << std::endl;
			}
			break;
			}
		};
		},
		g_client);
	g_client->Open((to_wide_string(pipeName)).c_str(), maxChannels);

	std::string text;
	while (1)
	{
		std::getline(std::cin, text);
		if (text == "C")

		{
			m_stop = TRUE;
			if (g_task1.joinable())
				g_task1.join();
			if (g_task2.joinable())
				g_task2.join();
			if (g_client != NULL)
				g_client->Close();

			break;
		}
		if (g_client != NULL)
			g_client->SendMessage(101, (const CHAR*)text.data(), text.size());
	}
	if (g_client != NULL)
		g_client->Close();
	if (g_task1.joinable())
		g_task1.join();
	if (g_task2.joinable())
		g_task2.join();
	if (g_client != NULL)
		g_client->Release();
    if (g_logFile.is_open())
        g_logFile.close();
}
