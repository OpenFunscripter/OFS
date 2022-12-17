#include "OFS_FunscriptMetadataEditor.h"
#include "OFS_Profiling.h"
#include "OFS_Localization.h"

#include "OFS_ImGui.h"
#include "imgui.h"
#include "imgui_stdlib.h"

#include "state/MetadataEditorState.h"

OFS_FunscriptMetadataEditor::OFS_FunscriptMetadataEditor() noexcept
{
    stateHandle = OFS_AppState<FunscriptMetadataState>::Register(FunscriptMetadataState::StateName);
}

bool OFS_FunscriptMetadataEditor::ShowMetadataEditor(bool* open, Funscript::Metadata& metadata) noexcept
{
    if(*open) ImGui::OpenPopup(TR_ID("METADATA_EDITOR", Tr::METADATA_EDITOR));
    OFS_PROFILE(__FUNCTION__);
    bool metaDataChanged = false;

    if (ImGui::BeginPopupModal(TR_ID("METADATA_EDITOR", Tr::METADATA_EDITOR), open, ImGuiWindowFlags_NoDocking)) {
        metaDataChanged |= ImGui::InputText(TR(TITLE), &metadata.title);
        Util::FormatTime(Util::FormatBuffer, sizeof(Util::FormatBuffer), (float)metadata.duration, false);
        ImGui::LabelText(TR(DURATION), "%s", Util::FormatBuffer);

        metaDataChanged |= ImGui::InputText(TR(CREATOR), &metadata.creator);
        metaDataChanged |= ImGui::InputText(TR(URL), &metadata.script_url);
        metaDataChanged |= ImGui::InputText(TR(VIDEO_URL), &metadata.video_url);
        metaDataChanged |= ImGui::InputTextMultiline(TR(DESCRIPTION), &metadata.description, ImVec2(0.f, ImGui::GetFontSize()*3.f));
        metaDataChanged |= ImGui::InputTextMultiline(TR(NOTES), &metadata.notes, ImVec2(0.f, ImGui::GetFontSize() * 3.f));

        {
            enum class LicenseType : int32_t {
                None,
                Free,
                Paid
            };
            static LicenseType currentLicense = LicenseType::None;

            auto licenseTypeToString = [](LicenseType type) -> const char*
            {
                switch (type)
                {
                    case LicenseType::None: return TR(NONE);
                    case LicenseType::Free: return TR(FREE);
                    case LicenseType::Paid: return TR(PAID);
                }
                return "";
            };

            if(ImGui::BeginCombo(TR(LICENSE), licenseTypeToString(currentLicense)))
            {
                if(ImGui::Selectable(TR(NONE), currentLicense == LicenseType::None))
                {
                    metadata.license = "";
                    currentLicense = LicenseType::None;
                    metaDataChanged = true;
                }
                if(ImGui::Selectable(TR(FREE), currentLicense == LicenseType::Free))
                {
                    metadata.license = "Free";
                    currentLicense = LicenseType::Free;
                    metaDataChanged = true;
                }
                if(ImGui::Selectable(TR(PAID), currentLicense == LicenseType::Paid))
                {
                    metadata.license = "Paid";
                    currentLicense = LicenseType::Paid;
                    metaDataChanged = true;
                }
                ImGui::EndCombo();
            }

            if (!metadata.license.empty()) {
                ImGui::SameLine(); ImGui::Text("-> \"%s\"", metadata.license.c_str());
            }
        }
    
        auto renderTagButtons = [&metaDataChanged](std::vector<std::string>& tags) {
            auto availableWidth = ImGui::GetContentRegionAvail().x;
            int removeIndex = -1;
            for (int i = 0; i < tags.size(); i++) {
                ImGui::PushID(i);
                auto& tag = tags[i];

                if (ImGui::Button(tag.c_str())) {
                    removeIndex = i;
                }
                auto nextLineCursor = ImGui::GetCursorPos();
                ImGui::SameLine();
                if (ImGui::GetCursorPosX() + ImGui::GetItemRectSize().x >= availableWidth) {
                    ImGui::SetCursorPos(nextLineCursor);
                }

                ImGui::PopID();
            }
            if (removeIndex != -1) {
                tags.erase(tags.begin() + removeIndex);
                metaDataChanged = true;
            }
        };

        constexpr const char* tagIdString = "##Tag";
        ImGui::TextUnformatted(TR(TAGS));
        static std::string newTag;
        auto addTag = [&metadata, &metaDataChanged, tagIdString](std::string& newTag) {
            Util::trim(newTag);
            if (!newTag.empty()) {
                metadata.tags.emplace_back(newTag); newTag.clear();
            }
            ImGui::ActivateItem(ImGui::GetID(tagIdString));
            metaDataChanged = true;
        };

        if (ImGui::InputText(tagIdString, &newTag, ImGuiInputTextFlags_EnterReturnsTrue)) {
            addTag(newTag);
        };
        ImGui::SameLine();
        if (ImGui::Button(TR(ADD), ImVec2(-1.f, 0.f))) { 
            addTag(newTag);
        }
    
        auto& style = ImGui::GetStyle();

        renderTagButtons(metadata.tags);
        ImGui::NewLine();

        constexpr const char* performerIdString = "##Performer";
        ImGui::TextUnformatted(TR(PERFORMERS));
        static std::string newPerformer;
        auto addPerformer = [&metadata, &metaDataChanged, performerIdString](std::string& newPerformer) {
            Util::trim(newPerformer);
            if (!newPerformer.empty()) {
                metadata.performers.emplace_back(newPerformer); newPerformer.clear(); 
            }
            auto performerID = ImGui::GetID(performerIdString);
            ImGui::ActivateItem(performerID);
            metaDataChanged = true;
        };
        if (ImGui::InputText(performerIdString, &newPerformer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            addPerformer(newPerformer);
        }
        ImGui::SameLine();
        if (ImGui::Button(TR_ID("ADD_PERFORMER", Tr::ADD), ImVec2(-1.f, 0.f))) {
            addPerformer(newPerformer);
        }

        renderTagButtons(metadata.performers);
        ImGui::NewLine();
        ImGui::Separator();
        float availWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(FMT("%s " ICON_COPY, TR(SAVE_TEMPLATE)), ImVec2(availWidth, 0.f))) {
            auto& state = FunscriptMetadataState::State(stateHandle);
            state.defaultMetadata = metadata;
        }
        OFS::Tooltip(TR(SAVE_TEMPLATE_TOOLTIP));
        ImGui::EndPopup();
    }
    return metaDataChanged;   
}