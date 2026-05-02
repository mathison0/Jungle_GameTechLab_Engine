// Implements the bottom Content Drawer browser.
#include "Editor/UI/EditorContentDrawerPanel.h"

#include "Component/CameraComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Platform/PlatformProcess.h"
#include "Platform/Paths.h"
#include "Serialization/SceneSaveManager.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>

namespace
{
constexpr float SidebarWidth = 230.0f;
constexpr float TileWidth = 148.0f;
constexpr float TileHeight = 86.0f;

const char* ContentAssetPayloadType = "CRASH_CONTENT_ASSET_PATH";
const char* LuaScriptPayloadType = "CRASH_LUA_SCRIPT_PATH";

FString ToLowerAscii(FString Value)
{
    std::transform(Value.begin(), Value.end(), Value.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Value;
}

FString PathDisplayName(const std::filesystem::path& Path)
{
    return FPaths::FromPath(Path.filename());
}

FString ExtensionLower(const std::filesystem::path& Path)
{
    return ToLowerAscii(FPaths::FromPath(Path.extension()));
}

FString NormalizePathKey(const std::filesystem::path& Path)
{
    return ToLowerAscii(FPaths::FromPath(Path.lexically_normal()));
}

bool PathEquals(const std::filesystem::path& A, const std::filesystem::path& B)
{
    return NormalizePathKey(A) == NormalizePathKey(B);
}

FString TrimToWidth(FString Text, float MaxWidth)
{
    if (ImGui::CalcTextSize(Text.c_str()).x <= MaxWidth)
    {
        return Text;
    }

    constexpr const char* Ellipsis = "...";
    while (!Text.empty())
    {
        Text.pop_back();
        FString Candidate = Text + Ellipsis;
        if (ImGui::CalcTextSize(Candidate.c_str()).x <= MaxWidth)
        {
            return Candidate;
        }
    }
    return Ellipsis;
}

const char* KindLabel(bool bDirectory, const FString& Extension)
{
    if (bDirectory)
    {
        return "[DIR]";
    }

    if (Extension == ".lua")
    {
        return "[LUA]";
    }
    if (Extension == ".json")
    {
        return "[MAT]";
    }
    if (Extension == ".obj" || Extension == ".fbx")
    {
        return "[MESH]";
    }
    if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".dds")
    {
        return "[IMG]";
    }
    if (Extension == ".scene")
    {
        return "[SCN]";
    }
    return "[FILE]";
}
} // namespace

void FEditorContentDrawerPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    EnsureInitialized();
    DrawToolbar();
    ImGui::Separator();

    if (ImGui::BeginChild("ContentDrawerSidebar", ImVec2(SidebarWidth, 0.0f), true))
    {
        DrawSidebar();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("ContentDrawerAssetArea", ImVec2(0.0f, 0.0f), false))
    {
        DrawAssetArea();
    }
    ImGui::EndChild();
}

void FEditorContentDrawerPanel::EnsureInitialized()
{
    if (bInitialized)
    {
        return;
    }

    SearchBuf.fill('\0');
    OpenDirectoryKeys.insert(NormalizePathKey(GetRootDirectory(EContentRoot::Content)));
    OpenDirectoryKeys.insert(NormalizePathKey(GetRootDirectory(EContentRoot::Scripts)));
    SetCurrentContentDirectory(EContentRoot::Content, GetRootDirectory(EContentRoot::Content));
    bInitialized = true;
}

void FEditorContentDrawerPanel::SetCurrentContentDirectory(EContentRoot Root, const std::filesystem::path& Directory)
{
    CurrentRoot = Root;

    std::error_code Ec;
    if (std::filesystem::exists(Directory, Ec) && std::filesystem::is_directory(Directory, Ec))
    {
        CurrentDirectory = Directory.lexically_normal();
        return;
    }

    CurrentDirectory = GetRootDirectory(Root).lexically_normal();
}

void FEditorContentDrawerPanel::DrawToolbar()
{
    ImGui::TextUnformatted("Content Drawer");
    ImGui::SameLine();

    const float SearchWidth = (std::max)(240.0f, ImGui::GetContentRegionAvail().x * 0.38f);
    ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - SearchWidth));
    ImGui::SetNextItemWidth(SearchWidth);
    ImGui::InputTextWithHint("##ContentDrawerSearch", "Search current folder", SearchBuf.data(), SearchBuf.size());
    if (bReclaimSearchFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        bReclaimSearchFocus = false;
    }
}

void FEditorContentDrawerPanel::DrawSidebar()
{
    DrawDirectoryNode(EContentRoot::Content, GetRootDirectory(EContentRoot::Content), "Content");
    DrawDirectoryNode(EContentRoot::Scripts, GetRootDirectory(EContentRoot::Scripts), "Scripts");
}

void FEditorContentDrawerPanel::DrawDirectoryNode(EContentRoot Root, const std::filesystem::path& Directory, const char* Label)
{
    const TArray<std::filesystem::path> Children = GetChildDirectories(Directory);
    const FString DirectoryKey = NormalizePathKey(Directory);
    const bool bHasChildren = !Children.empty();
    const bool bStoredOpen = OpenDirectoryKeys.contains(DirectoryKey);
    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!bHasChildren)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (IsCurrentDirectory(Root, Directory))
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(FPaths::FromPath(Directory).c_str());
    if (bHasChildren)
    {
        ImGui::SetNextItemOpen(bStoredOpen, ImGuiCond_Always);
    }
    const bool bOpen = ImGui::TreeNodeEx(Label, Flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        SetCurrentContentDirectory(Root, Directory);
    }
    if (bHasChildren && ImGui::IsItemToggledOpen())
    {
        if (bOpen)
        {
            OpenDirectoryKeys.insert(DirectoryKey);
        }
        else
        {
            OpenDirectoryKeys.erase(DirectoryKey);
        }
    }
    if (bHasChildren && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (bOpen)
        {
            OpenDirectoryKeys.erase(DirectoryKey);
        }
        else
        {
            OpenDirectoryKeys.insert(DirectoryKey);
        }
        SetCurrentContentDirectory(Root, Directory);
    }

    if (bOpen && bHasChildren)
    {
        for (const std::filesystem::path& Child : Children)
        {
            const FString ChildLabel = PathDisplayName(Child);
            DrawDirectoryNode(Root, Child, ChildLabel.c_str());
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void FEditorContentDrawerPanel::DrawAssetArea()
{
    DrawBreadcrumb();
    ImGui::Separator();

    TArray<FContentItem> Items = BuildCurrentItems();
    const int32 ItemCount = static_cast<int32>(Items.size());
    if (ItemCount == 0)
    {
        ImGui::TextDisabled(SearchBuf[0] != '\0' ? "No matching assets." : "This folder is empty.");
        return;
    }

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const int32 ColumnCount = (std::max)(1, static_cast<int32>(AvailableWidth / TileWidth));
    int32 ColumnIndex = 0;

    for (const FContentItem& Item : Items)
    {
        DrawItemTile(Item);

        ++ColumnIndex;
        if (ColumnIndex < ColumnCount)
        {
            ImGui::SameLine();
        }
        else
        {
            ColumnIndex = 0;
        }
    }
}

void FEditorContentDrawerPanel::DrawBreadcrumb()
{
    const std::filesystem::path RootPath = GetRootDirectory(CurrentRoot).lexically_normal();
    std::error_code Ec;
    std::filesystem::path RelativePath = std::filesystem::relative(CurrentDirectory, RootPath, Ec);
    if (Ec)
    {
        RelativePath.clear();
    }

    TArray<TPair<FString, std::filesystem::path>> Segments;
    Segments.push_back({ "Asset", RootPath });
    Segments.push_back({ GetRootLabel(CurrentRoot), RootPath });

    std::filesystem::path AccumulatedPath = RootPath;
    for (const std::filesystem::path& Part : RelativePath)
    {
        const FString PartName = FPaths::FromPath(Part);
        if (PartName.empty() || PartName == ".")
        {
            continue;
        }

        AccumulatedPath /= Part;
        Segments.push_back({ PartName, AccumulatedPath });
    }

    const ImVec2 StartPos = ImGui::GetCursorScreenPos();
    const float Height = ImGui::GetFrameHeight();
    const float Width = ImGui::GetContentRegionAvail().x;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    DrawList->AddRectFilled(
        StartPos,
        ImVec2(StartPos.x + Width, StartPos.y + Height),
        IM_COL32(28, 30, 34, 255),
        4.0f);
    DrawList->AddRect(
        StartPos,
        ImVec2(StartPos.x + Width, StartPos.y + Height),
        IM_COL32(48, 51, 58, 255),
        4.0f);

    ImGui::SetCursorScreenPos(ImVec2(StartPos.x + 8.0f, StartPos.y + 3.0f));

    for (size_t Index = 0; Index < Segments.size(); ++Index)
    {
        const TPair<FString, std::filesystem::path>& Segment = Segments[Index];
        const bool bLast = Index + 1 == Segments.size();
        const FString ButtonLabel = Segment.first + "##Breadcrumb" + std::to_string(Index);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, bLast ? ImVec4(0.18f, 0.22f, 0.27f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.29f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.25f, 0.31f, 1.0f));
        if (ImGui::SmallButton(ButtonLabel.c_str()))
        {
            SetCurrentContentDirectory(CurrentRoot, Segment.second);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        if (!bLast)
        {
            ImGui::SameLine(0.0f, 5.0f);
            ImGui::TextColored(ImVec4(0.52f, 0.56f, 0.62f, 1.0f), ">");
            ImGui::SameLine(0.0f, 5.0f);
        }
    }

    const float EndY = StartPos.y + Height;
    if (ImGui::GetCursorScreenPos().y < EndY)
    {
        ImGui::SetCursorScreenPos(ImVec2(StartPos.x, EndY + ImGui::GetStyle().ItemSpacing.y));
    }
}

void FEditorContentDrawerPanel::DrawItemTile(const FContentItem& Item)
{
    ImGui::PushID(Item.ProjectRelativePath.c_str());

    const ImVec2 TileSize(TileWidth - ImGui::GetStyle().ItemSpacing.x, TileHeight);
    const ImVec2 TilePos = ImGui::GetCursorScreenPos();
    const bool bPressed = ImGui::InvisibleButton("##ContentItemTile", TileSize);
    const bool bHovered = ImGui::IsItemHovered();

    (void)bPressed;

    if (Item.bDirectory && bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        SetCurrentContentDirectory(CurrentRoot, Item.FullPath);
    }
    else if (!Item.bDirectory && Item.Extension == ".scene" && bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        LoadSceneFile(Item.FullPath);
    }
    else if (!Item.bDirectory && CurrentRoot == EContentRoot::Scripts && Item.Extension == ".lua" && bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (!FPlatformProcess::OpenFile(Item.FullPath))
        {
            const FString FilePath = FPaths::FromPath(Item.FullPath);
            UE_LOG(EditorUI, Warning, "Failed to open Lua script: %s", FilePath.c_str());
        }
    }

    if (!Item.bDirectory && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        const bool bLuaScript = CurrentRoot == EContentRoot::Scripts && Item.Extension == ".lua";
        const FString& PayloadText = bLuaScript ? Item.RootRelativePath : Item.ProjectRelativePath;
        ImGui::SetDragDropPayload(
            bLuaScript ? LuaScriptPayloadType : ContentAssetPayloadType,
            PayloadText.c_str(),
            PayloadText.size() + 1);

        ImGui::TextUnformatted(KindLabel(Item.bDirectory, Item.Extension));
        ImGui::TextUnformatted(Item.Name.c_str());
        ImGui::TextDisabled("%s", PayloadText.c_str());
        ImGui::EndDragDropSource();
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 TileMax(TilePos.x + TileSize.x, TilePos.y + TileSize.y);
    const ImU32 BgColor = bHovered ? IM_COL32(47, 51, 57, 255) : IM_COL32(34, 36, 40, 255);
    const ImU32 BorderColor = bHovered ? IM_COL32(82, 124, 190, 255) : IM_COL32(56, 58, 64, 255);
    const ImU32 IconColor = Item.bDirectory ? IM_COL32(236, 186, 80, 255) : IM_COL32(155, 185, 225, 255);
    const ImU32 TextColor = IM_COL32(225, 225, 225, 255);
    const ImU32 MutedTextColor = IM_COL32(150, 154, 162, 255);

    DrawList->AddRectFilled(TilePos, TileMax, BgColor, 4.0f);
    DrawList->AddRect(TilePos, TileMax, BorderColor, 4.0f);

    const float TextMaxWidth = TileSize.x - 16.0f;
    const FString Kind = KindLabel(Item.bDirectory, Item.Extension);
    const FString Name = TrimToWidth(Item.Name, TextMaxWidth);
    const FString PathText = TrimToWidth(Item.RootRelativePath, TextMaxWidth);

    DrawList->AddText(ImVec2(TilePos.x + 10.0f, TilePos.y + 10.0f), IconColor, Kind.c_str());
    DrawList->AddText(ImVec2(TilePos.x + 10.0f, TilePos.y + 34.0f), TextColor, Name.c_str());
    DrawList->AddText(ImVec2(TilePos.x + 10.0f, TilePos.y + 58.0f), MutedTextColor, PathText.c_str());

    ImGui::PopID();
}

void FEditorContentDrawerPanel::LoadSceneFile(const std::filesystem::path& ScenePath)
{
    if (!EditorEngine)
    {
        UE_LOG(EditorUI, Warning, "Load scene requested without editor engine.");
        return;
    }

    const FString FilePath = FPaths::FromPath(ScenePath);
    EditorEngine->StopPlayInEditorImmediate();
    EditorEngine->ClearScene();

    FWorldContext LoadCtx;
    FPerspectiveCameraData CamData;
    FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, CamData);
    if (!LoadCtx.World)
    {
        UE_LOG(EditorUI, Error, "Scene load failed: %s", FilePath.c_str());
        EditorEngine->ResetViewport();
        return;
    }

    EditorEngine->GetWorldList().push_back(LoadCtx);
    EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
    EditorEngine->GetSelectionManager().SetWorld(LoadCtx.World);
    LoadCtx.World->WarmupPickingData();
    EditorEngine->ResetViewport();

    if (CamData.bValid)
    {
        for (FLevelEditorViewportClient* ViewportClient : EditorEngine->GetLevelViewportClients())
        {
            if (ViewportClient->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
                ViewportClient->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
            {
                if (UCameraComponent* Camera = ViewportClient->GetCamera())
                {
                    Camera->SetWorldLocation(CamData.Location);
                    Camera->SetRelativeRotation(CamData.Rotation);
                    FCameraState CameraState = Camera->GetCameraState();
                    CameraState.FOV = CamData.FOV;
                    CameraState.NearZ = CamData.NearClip;
                    CameraState.FarZ = CamData.FarClip;
                    Camera->SetCameraState(CameraState);
                }
                break;
            }
        }
    }

    UE_LOG(EditorUI, Info, "Scene loaded: %s", FilePath.c_str());
}

TArray<FEditorContentDrawerPanel::FContentItem> FEditorContentDrawerPanel::BuildCurrentItems() const
{
    TArray<FContentItem> Items;

    std::error_code Ec;
    if (!std::filesystem::exists(CurrentDirectory, Ec) || !std::filesystem::is_directory(CurrentDirectory, Ec))
    {
        return Items;
    }

    const bool bSearch = SearchBuf[0] != '\0';
    if (bSearch)
    {
        std::filesystem::recursive_directory_iterator It(CurrentDirectory, Ec);
        std::filesystem::recursive_directory_iterator End;
        while (!Ec && It != End)
        {
            const std::filesystem::directory_entry Entry = *It;
            const bool bDirectory = Entry.is_directory(Ec);
            const bool bRegularFile = Entry.is_regular_file(Ec);
            if (bDirectory || bRegularFile)
            {
                FContentItem Item;
                Item.FullPath = Entry.path().lexically_normal();
                Item.Name = PathDisplayName(Item.FullPath);
                Item.Extension = bDirectory ? "" : ExtensionLower(Item.FullPath);
                Item.RootRelativePath = MakeRootRelativePath(CurrentRoot, Item.FullPath);
                Item.ProjectRelativePath = MakeProjectRelativePath(Item.FullPath);
                Item.bDirectory = bDirectory;
                if (MatchesSearch(Item))
                {
                    Items.push_back(std::move(Item));
                }
            }
            It.increment(Ec);
        }
    }
    else
    {
        for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(CurrentDirectory, Ec))
        {
            if (Ec)
            {
                break;
            }

            const bool bDirectory = Entry.is_directory(Ec);
            const bool bRegularFile = Entry.is_regular_file(Ec);
            if (!bDirectory && !bRegularFile)
            {
                continue;
            }

            FContentItem Item;
            Item.FullPath = Entry.path().lexically_normal();
            Item.Name = PathDisplayName(Item.FullPath);
            Item.Extension = bDirectory ? "" : ExtensionLower(Item.FullPath);
            Item.RootRelativePath = MakeRootRelativePath(CurrentRoot, Item.FullPath);
            Item.ProjectRelativePath = MakeProjectRelativePath(Item.FullPath);
            Item.bDirectory = bDirectory;
            Items.push_back(std::move(Item));
        }
    }

    std::sort(Items.begin(), Items.end(), [](const FContentItem& A, const FContentItem& B)
              {
                  if (A.bDirectory != B.bDirectory)
                  {
                      return A.bDirectory > B.bDirectory;
                  }
                  return ToLowerAscii(A.Name) < ToLowerAscii(B.Name);
              });

    return Items;
}

TArray<std::filesystem::path> FEditorContentDrawerPanel::GetChildDirectories(const std::filesystem::path& Directory) const
{
    TArray<std::filesystem::path> Directories;

    std::error_code Ec;
    if (!std::filesystem::exists(Directory, Ec) || !std::filesystem::is_directory(Directory, Ec))
    {
        return Directories;
    }

    for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Directory, Ec))
    {
        if (Ec)
        {
            break;
        }

        if (Entry.is_directory(Ec))
        {
            Directories.push_back(Entry.path().lexically_normal());
        }
    }

    std::sort(Directories.begin(), Directories.end(), [](const std::filesystem::path& A, const std::filesystem::path& B)
              {
                  return ToLowerAscii(PathDisplayName(A)) < ToLowerAscii(PathDisplayName(B));
              });
    return Directories;
}

std::filesystem::path FEditorContentDrawerPanel::GetRootDirectory(EContentRoot Root) const
{
    return Root == EContentRoot::Scripts
               ? FPaths::ToPath(FPaths::ScriptsDir()).lexically_normal()
               : FPaths::ToPath(FPaths::ContentDir()).lexically_normal();
}

const char* FEditorContentDrawerPanel::GetRootLabel(EContentRoot Root) const
{
    return Root == EContentRoot::Scripts ? "Scripts" : "Content";
}

FString FEditorContentDrawerPanel::MakeRootRelativePath(EContentRoot Root, const std::filesystem::path& Path) const
{
    const std::filesystem::path RootPath = GetRootDirectory(Root);

    std::error_code Ec;
    std::filesystem::path Relative = std::filesystem::relative(Path, RootPath, Ec);
    if (Ec || Relative.empty())
    {
        Relative = Path.filename();
    }

    return FPaths::FromPath(Relative);
}

FString FEditorContentDrawerPanel::MakeProjectRelativePath(const std::filesystem::path& Path) const
{
    return FPaths::MakeRelativeToRoot(Path);
}

bool FEditorContentDrawerPanel::IsCurrentDirectory(EContentRoot Root, const std::filesystem::path& Directory) const
{
    return CurrentRoot == Root && PathEquals(CurrentDirectory, Directory);
}

bool FEditorContentDrawerPanel::MatchesSearch(const FContentItem& Item) const
{
    const FString Query = ToLowerAscii(SearchBuf.data());
    if (Query.empty())
    {
        return true;
    }

    const FString Name = ToLowerAscii(Item.Name);
    const FString RootRelativePath = ToLowerAscii(Item.RootRelativePath);
    return Name.find(Query) != FString::npos || RootRelativePath.find(Query) != FString::npos;
}
