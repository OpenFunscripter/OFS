///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#include <EABase/eabase.h>
#include <EAAssert/eaassert.h>
#include <EAMain/internal/EAMainChannels.h>

#if defined(EA_PLATFORM_IPHONE)
    #define EAMAIN_HAS_NETWORK_CHANNEL 1
    #define EAMAIN_FREOPEN_SUPPORTED 1
#endif

#if defined(EA_PLATFORM_WINRT)
    #define EAMAIN_HAS_NETWORK_CHANNEL 1
    #define EAMAIN_FREOPEN_SUPPORTED 0
#endif

#if defined(EA_PLATFORM_WINDOWS_PHONE) && !defined(EAMAIN_HAS_NETWORK_CHANNEL)
    #define EAMAIN_HAS_NETWORK_CHANNEL 1
    #define EAMAIN_FREOPEN_SUPPORTED 0
#endif

// winrt-arm configurations do not have winsock, so for these configurations
// we disable the use of the network channel.
#if defined(_MSC_VER) && !defined(EA_PLATFORM_WINDOWS_PHONE) && defined(EA_PROCESSOR_ARM) && defined(EAMAIN_HAS_NETWORK_CHANNEL)
    #undef EAMAIN_HAS_NETWORK_CHANNEL
#endif

#if defined(EA_PLATFORM_CAPILANO) && !defined(EAMAIN_HAS_NETWORK_CHANNEL)
    #define EAMAIN_HAS_NETWORK_CHANNEL 1
    #define EAMAIN_FREOPEN_SUPPORTED 1
#endif

#if !defined(EAMAIN_HAS_NETWORK_CHANNEL)
    #define EAMAIN_HAS_NETWORK_CHANNEL 0
#endif

#if !defined(EAMAIN_FREOPEN_SUPPORTED)
    #define EAMAIN_FREOPEN_SUPPORTED 0
#endif

#if EAMAIN_HAS_NETWORK_CHANNEL

#if defined(_MSC_VER)
    EA_DISABLE_ALL_VC_WARNINGS()

    #if defined(WINAPI_FAMILY)
        #undef WINAPI_FAMILY
    #endif

    #define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

    #include <WinSock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Ws2_32.lib")

    #if defined(EA_PLATFORM_WINDOWS_PHONE)
        #pragma warning(disable:4265)
        #include <thread>
    #else
        // Restoring VC warnings on Windows Phone causes some warnings to pop
        // up down below, emanating from std::thread. In order to have some
        // coverage of this code, warnings are re-enabled on other platforms.
        EA_RESTORE_ALL_VC_WARNINGS()
    #endif

    #define SocketGetLastError() WSAGetLastError()
    #define snprintf _snprintf
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <errno.h>
#if !defined(EA_PLATFORM_SONY)
    #include <netdb.h>
#else
    #include <net.h>
#endif
    #include <string.h>
    #include <unistd.h>
    typedef int SOCKET;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
    #define SD_SEND SHUT_WR
    #define closesocket close
    #define WSAECONNREFUSED ECONNREFUSED
    #define WSAENETUNREACH ENETUNREACH
    #define SocketGetLastError() errno
#endif

#include <stdio.h>
#include "eathread/eathread_thread.h"
#include "EAStdC/EAMemory.h"
#include "EAStdC/EAString.h"
#include "EAStdC/EASprintf.h"

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            class NetworkChannel : public IChannel
            {
                SOCKET m_socket;
                char m_serverAddressStorage[128];
                char m_port[6];
                const char *m_serverAddress;

                static void SleepThread(int milliseconds);

            public:
                NetworkChannel();

                virtual ~NetworkChannel();
                virtual void Init();
                virtual void Send(const char8_t *data);
                virtual void Shutdown();

                void SetServerPort(const char *server, const char *port);
                bool Connect();
            };

            static NetworkChannel g_NetworkChannelInstance;

            void NetworkChannel::SleepThread(int milliseconds)
            {
            #if defined(_MSC_VER)
                #if defined(EA_PLATFORM_WINDOWS_PHONE)
                    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
                #else
                    Sleep(milliseconds);
                #endif
            #else
                usleep(milliseconds * 1000);
            #endif
            }

            NetworkChannel::NetworkChannel()
                : m_socket(INVALID_SOCKET)
                , m_serverAddress(&m_serverAddressStorage[0])
            {
                EA::StdC::Memset8(m_serverAddressStorage, 0, sizeof m_serverAddressStorage);
                EA::StdC::Memset8(m_port, 0, sizeof m_port);

#if defined(_MSC_VER)
                WSAData wsaData = {};
                WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            }

            static void LogNetworkError(const char *format, ...)
            {
                (void)format;
            }

            NetworkChannel::~NetworkChannel()
            {
#if defined(_MSC_VER)
                WSACleanup();
#endif
            }

            void NetworkChannel::Init()
            {
            }

            void NetworkChannel::Send(const char8_t *data)
            {
                char *buffer = const_cast<char *>(data);
                ssize_t bufferLength = static_cast<ssize_t>(strlen(buffer));
                ssize_t bytesSent = 0;
                while (bytesSent < bufferLength)
                {
                    ssize_t result = send(m_socket, buffer + bytesSent, static_cast<int>(bufferLength - bytesSent), 0);

                    if (result == SOCKET_ERROR)
                    {
                        bool reconnected = false;
                        LogNetworkError("[NetworkChannel::Send] Reconnecting...");

                        for (int i = 0; i < 20; ++i)
                        {
                            Shutdown();

                            if (!Connect())
                            {
                                LogNetworkError("[NetworkChannel::Send] Send failed: %d", SocketGetLastError());
                            }
                            else
                            {
                                reconnected = true;
                                break;
                            }

                            SleepThread(500);
                        }

                        if (!reconnected)
                        {
                            LogNetworkError("[NetworkChannel::Send] Unable to connect, aborting.");
                            return;
                        }
                    }
                    else
                    {
                        bytesSent += result;
                    }
                }
            }

            void NetworkChannel::Shutdown()
            {
                // Closing the socket does not wait for pending data to be sent.
                // To ensure that all data has been sent, the write end of the
                // socket must first be shut down. This sends a FIN packet to
                // the receiver after all data has been sent and acknowledged
                // by the receiver. This must also be paired with a call to
                // recv to ensure that all pending readable data has been
                // read.
                shutdown(m_socket, SD_SEND);

                char buffer[128];

                for (;;)
                {
                    ssize_t rv = recv(m_socket, buffer, (int)sizeof buffer, 0);

                    if (rv <= 0)
                    {
                        // We can assume that a graceful shutdown has occurred
                        // when rv == 0. If rv < 0 an error has occurred, but
                        // we are not at a point where we can easily recover, so
                        // this code will just shutdown the socket and exit the
                        // program if an error happens here.
                        break;
                    }
                }

                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
            }

            void NetworkChannel::SetServerPort(const char *server, const char *port)
            {
                using namespace EA::StdC;

                // If, somehow, the server name string is longer than the storge
                // allocated, allocate some space for it on the C-heap. Not ideal,
                // may actually fail some tests, but the alternative would be
                // no output from EAMain at all.
                if (Strlcpy(m_serverAddressStorage, server, sizeof m_serverAddressStorage) > (sizeof m_serverAddressStorage))
                {
                    size_t serverNameLength = Strlen(server);

                    char *ptr = static_cast<char *>(calloc(serverNameLength + 1, 1));
                    Strlcpy(ptr, server, serverNameLength + 1);

                    m_serverAddress = ptr;
                }

                // Valid port numbers are in the range [1, 65535] so will have
                // always 5 digits max.
                EA_ASSERT(sizeof m_port == 6);
                Strlcpy(m_port, port, sizeof m_port);
            }

            bool NetworkChannel::Connect()
            {
                EA_ASSERT(m_serverAddress != NULL);
                EA_ASSERT(m_port != NULL);

                bool result = false;
                SOCKET remoteSocket = INVALID_SOCKET;
                const int MAX_RETRIES = 250;

#if !defined(EA_PLATFORM_SONY)
                struct addrinfo hints = {};
                hints.ai_socktype = SOCK_STREAM;
                addrinfo *p = NULL;

                if (getaddrinfo(m_serverAddress, m_port, &hints, &p) != 0)
                {
                    LogNetworkError("[NetworkChannel::Connect] Cannot connect to %s:%s", m_serverAddress, m_port);
                    goto ErrorReturn;
                }
                for (struct addrinfo *endpoint = p; endpoint != NULL; endpoint = endpoint->ai_next)
#endif

                {
#if !defined(EA_PLATFORM_SONY)
                    remoteSocket = socket(endpoint->ai_family, endpoint->ai_socktype, endpoint->ai_protocol);
#else
                    remoteSocket = sceNetSocket("EAMain Socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
#endif
                    if (remoteSocket == INVALID_SOCKET)
                    {
                        LogNetworkError("[NetworkChannel::Connect] Cannot create socket");
                        goto ErrorReturn;
                    }

#if defined(_MSC_VER)
                    if (endpoint->ai_family == AF_INET6)
                    {
                        DWORD v6only = 0;
                        setsockopt(remoteSocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6only, sizeof v6only);
                    }
#endif

                    for (int i = 0; i < MAX_RETRIES; ++i)
                    {
#if !defined(EA_PLATFORM_SONY)
                        int connectResult = connect(remoteSocket, endpoint->ai_addr, (int)endpoint->ai_addrlen);
#else
                        char *p;
                        SceNetInPort_t port = strtol(m_port, &p, 10);

                        SceNetSockaddrIn sin;
                        EA::StdC::Memset8(&sin, 0, sizeof sin);
                        sin.sin_len = sizeof(sin);
                        sin.sin_family = SCE_NET_AF_INET;
                        if (sceNetInetPton(SCE_NET_AF_INET, m_serverAddress, &sin.sin_addr) <= 0)
                        {
                            LogNetworkError("[NetworkChannel::Connect] Cannot connect to %s:%s", m_serverAddress, m_port);
                            goto ErrorReturn;
                        }

                        sin.sin_port = sceNetHtons(port);
                        sin.sin_vport = sceNetHtons(SCE_NET_ADHOC_PORT);
                        int connectResult = sceNetConnect(remoteSocket, (SceNetSockaddr *)&sin, sizeof(sin));
#endif
                        if (connectResult == 0)
                        {
                            result = true;
                            m_socket = remoteSocket;
                            goto SuccessReturn;
                        }

                        switch (SocketGetLastError())
                        {
                            case WSAENETUNREACH:
                                SleepThread(20);
                                continue;
                            default:
                                LogNetworkError("[NetworkChannel::Connect] Cannot connect to socket");
                                goto ErrorReturn;
                        }
                    }
                }

            ErrorReturn:
                if (remoteSocket != INVALID_SOCKET)
                {
                    LogNetworkError("[NetworkChannel::Connect] FAILED");
                    closesocket(remoteSocket);
                }

            SuccessReturn:
#if !defined(EA_PLATFORM_SONY)
                if (p)
                {
                    freeaddrinfo(p);
                }
#endif
                return result;
            }

            static IChannel *CreateNetworkChannelImpl(const char *server, int port)
            {
                char portString[6];

                if (port > 65536 || port < 0)
                {
                    return NULL;
                }

                snprintf(portString, sizeof portString, "%d", port);
                portString[5] = 0;

                g_NetworkChannelInstance.SetServerPort(server, portString);

                if (g_NetworkChannelInstance.Connect())
                {
                    return &g_NetworkChannelInstance;
                }

                return NULL;
            }

#if !EAMAIN_FREOPEN_SUPPORTED
            IChannel *CreateNetworkChannel(const char *server, int port)
            {
                // On platforms where we do not support freopen on standard
                // IO streams, we create a raw network channel.
                return CreateNetworkChannelImpl(server, port);
            }
#else
            static EA::Thread::Thread g_PrintThread;
            static volatile bool g_PrintThreadDone;
            static FILE *g_RedirectedStdoutHandle;
            static FILE *g_RedirectedStderrHandle;

            class StdoutWrapperChannel : public IChannel
            {
                NetworkChannel &m_channel;
                bool m_shutdown;

                StdoutWrapperChannel& operator=(const StdoutWrapperChannel&);
                StdoutWrapperChannel(const StdoutWrapperChannel&);

            public:
                StdoutWrapperChannel(NetworkChannel &channel)
                    : m_channel(channel)
                    , m_shutdown(false)
                {
                }

                virtual ~StdoutWrapperChannel();

                virtual void Init()
                {
                }

                virtual void Send(const char8_t *data)
                {
                    fputs(data, stdout);
                    fflush(stdout);
                }

                virtual void Shutdown()
                {
                    if (g_RedirectedStdoutHandle)
                    {
                        fclose(g_RedirectedStdoutHandle);
                    }

                    if (g_RedirectedStderrHandle)
                    {
                        fclose(g_RedirectedStderrHandle);
                    }

                    g_PrintThreadDone = true;

                    if (g_PrintThread.GetStatus() == EA::Thread::Thread::kStatusRunning)
                    {
                        g_PrintThread.WaitForEnd();
                    }

                    m_channel.Shutdown();
                    m_shutdown = true;
                }
            };

            StdoutWrapperChannel::~StdoutWrapperChannel()
            {
                if (!m_shutdown)
                {
                    Shutdown();
                }
            }

            static bool ReadFromFile(FILE *file)
            {
                static const int BUFFER_SIZE = 1024;
                static char buffer[BUFFER_SIZE + 1];
                bool haveRead = false;

                // This is a tortuous way of checking to see if there is data
                // available but the goal here is to prevent this thread from
                // blocking if nothing is ready. Blocking could be desirable
                // if we were only reading from one stream but because this
                // code attempts to read from both stdout and stderr we do not
                // want it to block on reading one while the other is receiving
                // important information.

                // Another possibility would be to use non-blocking IO but this
                // would require different implementations for different
                // platforms.
                long currentPosition = ftell(file);
                fseek(file, 0, SEEK_END);
                long endPosition = ftell(file);

                if (endPosition > currentPosition)
                {
                    size_t bytesAvailable = static_cast<size_t>(endPosition - currentPosition);

                    fseek(file, currentPosition, SEEK_SET);
                    while (bytesAvailable > 0)
                    {
                        size_t bytesToRead = (bytesAvailable > BUFFER_SIZE) ? BUFFER_SIZE : bytesAvailable;

                        size_t bytesRead = fread(buffer, 1, bytesToRead, file);
                        buffer[bytesRead] = 0;

                        g_NetworkChannelInstance.Send(buffer);

                        bytesAvailable -= bytesRead;
                    }

                    haveRead = true;
                }

                return haveRead;
            }

            static char g_StdoutLogPath[256];
            static char g_StderrLogPath[256];

            static intptr_t PrintFunction(void *)
            {
                FILE *stdoutLog = fopen(g_StdoutLogPath, "rb");
                FILE *stderrLog = fopen(g_StderrLogPath, "rb");

                while (!g_PrintThreadDone)
                {
                    // It might look neater to combine these file reads into
                    // the if statement but this was not done because the
                    // shortcircuting of the left hand condition prevented
                    // the right hand condition from being evaluated, but
                    // every iteration should read from both sources.
                    bool haveReadStdout = ReadFromFile(stdoutLog);
                    bool haveReadStderr = ReadFromFile(stderrLog);

                    if (!haveReadStdout && !haveReadStderr)
                    {
                        EA::Thread::ThreadSleep(50);
                    }
                }

                fflush(stdout); ReadFromFile(stdoutLog); fclose(stdoutLog);
                fflush(stderr); ReadFromFile(stderrLog); fclose(stderrLog);

                return 0;
            }

            IChannel *CreateNetworkChannel(const char *server, int port)
            {
#if defined(EA_PLATFORM_CAPILANO)
                if (IsDebuggerPresent())
                {
                    return NULL;
                }
#endif

                EA::Thread::ThreadParameters threadParameters;

                char *STDOUT_LOG_NAME = "stdout.log";
                const char *STDERR_LOG_NAME = "stderr.log";

                threadParameters.mnPriority = EA::Thread::kThreadPriorityMax;

                #if defined EA_PLATFORM_IPHONE
                    char temporaryDirectory[256];
                    confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof temporaryDirectory);

                    EA::StdC::Snprintf(g_StdoutLogPath, sizeof g_StdoutLogPath, "%s/%s", temporaryDirectory, STDOUT_LOG_NAME);
                    EA::StdC::Snprintf(g_StderrLogPath, sizeof g_StderrLogPath, "%s/%s", temporaryDirectory, STDERR_LOG_NAME);
                #elif defined EA_PLATFORM_CAPILANO
                    EA::StdC::Snprintf(g_StdoutLogPath, sizeof g_StdoutLogPath, "T:\\%s", STDOUT_LOG_NAME);
                    EA::StdC::Snprintf(g_StderrLogPath, sizeof g_StderrLogPath, "T:\\%s", STDERR_LOG_NAME);
                #else
                    EA::StdC::Strlcpy(g_StdoutLogPath, STDOUT_LOG_NAME, sizeof g_StdoutLogPath);
                    EA::StdC::Strlcpy(g_StderrLogPath, STDERR_LOG_NAME, sizeof g_StderrLogPath);
                #endif

                fprintf(stdout, ""); fflush(stdout);
                fprintf(stderr, ""); fflush(stderr);

                g_RedirectedStdoutHandle = freopen(g_StdoutLogPath, "wb", stdout);
                setvbuf(stdout, NULL, _IONBF, 0);
                g_RedirectedStderrHandle = freopen(g_StderrLogPath, "wb", stderr);
                setvbuf(stderr, NULL, _IONBF, 0);

                if (CreateNetworkChannelImpl(server, port) != NULL)
                {
                    g_PrintThread.Begin(PrintFunction, NULL, &threadParameters);
                }

                return new StdoutWrapperChannel(g_NetworkChannelInstance);
            }
#endif
        }
    }
}

#else

namespace EA
{
    namespace EAMain
    {
        namespace Internal
        {
            IChannel *CreateNetworkChannel(const char *server, int port)
            {
                return NULL;
            }
        }
    }
}

#endif
