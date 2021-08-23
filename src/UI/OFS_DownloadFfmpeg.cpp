#include "OFS_DownloadFfmpeg.h"
#include "OFS_Util.h"
#include "SDL_thread.h"

#include <sstream>

#ifdef WIN32
    #include <Windows.h>
    #include <urlmon.h>
    #pragma comment(lib, "urlmon.lib")

    class DownloadStatusCallback : public IBindStatusCallback
    {
    public:
        float Progress = 0.f;
        bool Stopped = false;

        HRESULT __stdcall QueryInterface(const IID &,void **) { 
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef(void) { 
            return 1;
        }
        ULONG STDMETHODCALLTYPE Release(void) {
            return 1;
        }
        HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dwReserved, IBinding *pib) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG *pnPriority) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved) {
            return S_OK;
        }
        virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult, LPCWSTR szError) {
            Stopped = true;
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *grfBINDF, BINDINFO *pbindinfo) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed) {
            return E_NOTIMPL;
        }        
        virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid, IUnknown *punk) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
        {
            Progress = (double)ulProgress/(double)ulProgressMax;
            return S_OK;
        }
    };
    static DownloadStatusCallback cb;
#endif

ImGuiID OFS_DownloadFfmpeg::ModalId = 0;
bool OFS_DownloadFfmpeg::FfmpegMissing = false;

void OFS_DownloadFfmpeg::DownloadFfmpegModal() noexcept
{
#ifdef WIN32
    static bool DownloadInProgress = false;
    static bool ExtractFailed = false;
    static auto ZipExists = Util::FileExists((Util::Basepath() / "ffmpeg.zip").u8string());

    bool isOpen = true;
    if(ImGui::BeginPopupModal(OFS_DownloadFfmpeg::ModalText, DownloadInProgress ? NULL : &isOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        if(DownloadInProgress) ImGui::ProgressBar(cb.Progress);
        if(!ZipExists && !DownloadInProgress) {
            ImGui::TextUnformatted("ffmpeg.exe was not found.\nDo you want to download it?");
            if(ImGui::Button("Yes", ImVec2(-1.f, 0.f))) {
                auto dlThread = [](void* data) -> int {
                    auto path = Util::Basepath();
                    auto downloadPath = (path / "ffmpeg.zip").u8string();
                    URLDownloadToFile(NULL, "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip",
                        downloadPath.c_str(), 0, &cb);
                    return 0;
                };
                DownloadInProgress = true;
                SDL_DetachThread(SDL_CreateThread(dlThread, "downloadFfmpeg", NULL));
            }
        }

        if(cb.Stopped && (cb.Progress < 1.f || std::isnan(cb.Progress))) {
            Util::MessageBoxAlert("Error", "Failed to download from https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip");
            FfmpegMissing = false;
            ImGui::CloseCurrentPopup();
        }

        if(ZipExists || cb.Progress >= 1.f) {
            DownloadInProgress = false;
            ZipExists = true;
            ImGui::TextUnformatted("Extracting uses tar.exe.\nIt will fail if it's not on your system.");
            ImGui::TextDisabled("tar.exe has been part of Windows 10 since build 17063");
            if(ImGui::Button("Extract", ImVec2(-1.f, 0.f))) {
                std::wstringstream ss;
                auto path = Util::Basepath();
                auto downloadPath = (path / "ffmpeg.zip");

				ss << L"tar.exe -xvf ";
				ss << L'"' << downloadPath.wstring() << L'"';
				ss << L" --strip-components 2  **/ffmpeg.exe";

				auto extractScript = ss.str();
				ExtractFailed = !_wsystem(extractScript.c_str()) == 0;
                if(!ExtractFailed) {
                    FfmpegMissing = false;
                    ImGui::CloseCurrentPopup();
                    Util::MessageBoxAlert("Done.", "ffmpeg.exe was successfully extracted.");
                }
            }
            if(ExtractFailed) {
                ImGui::TextUnformatted("Failed to extract ffmpeg.exe.");
            }
        }
        ImGui::EndPopup();
    }

    if(!isOpen) {
        FfmpegMissing = false;
    }
#endif // WIN32
}