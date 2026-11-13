#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <limits>
#include <cstdlib>
#include "Fantasy/World.h"
#include "Fantasy/MapCamera.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace fantasy;
namespace {
constexpr wchar_t MainClass[]=L"FantasyChronicleMainWindow";
constexpr wchar_t DetailClass[]=L"FantasyChronicleDetailWindow";
constexpr wchar_t PlacementKey[]=L"Software\FantasyChronicle";
constexpr wchar_t PlacementName[]=L"MainWindowPlacement";
struct WindowGeometry {
    DWORD version{1};
    LONG left{},top{},right{},bottom{};
    DWORD maximized{};
};
struct InitialGeometry { RECT rect{}; bool maximized{}; };

RECT workingAreaFor(const RECT* saved=nullptr) {
    MONITORINFO info{};info.cbSize=sizeof(info);
    HMONITOR monitor=saved?MonitorFromRect(saved,MONITOR_DEFAULTTONEAREST):
        MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY);
    if(GetMonitorInfoW(monitor,&info)) return info.rcWork;
    return RECT{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
}
InitialGeometry startupGeometry() {
    const RECT work=workingAreaFor();
    const int width=std::max(1,static_cast<int>(work.right-work.left)*4/5);
    const int height=std::max(1,static_cast<int>(work.bottom-work.top)*4/5);
    InitialGeometry result{{work.left+((work.right-work.left)-width)/2,
        work.top+((work.bottom-work.top)-height)/2,0,0},false};
    result.rect.right=result.rect.left+width;
    result.rect.bottom=result.rect.top+height;
    WindowGeometry saved{};
    DWORD bytes=static_cast<DWORD>(sizeof(saved)),type=0;
    if(RegGetValueW(HKEY_CURRENT_USER,PlacementKey,PlacementName,RRF_RT_REG_BINARY,
                    &type,&saved,&bytes)!=ERROR_SUCCESS || bytes!=sizeof(saved) || saved.version!=1) return result;
    RECT requested{saved.left,saved.top,saved.right,saved.bottom};
    if(requested.right-requested.left<540 || requested.bottom-requested.top<360 ||
       requested.right-requested.left>20000 || requested.bottom-requested.top>20000) return result;
    const RECT target=workingAreaFor(&requested);
    // Keep restored windows usable after changing the screen or monitor layout.
    const int w=std::min(static_cast<int>(requested.right-requested.left),static_cast<int>(target.right-target.left));
    const int h=std::min(static_cast<int>(requested.bottom-requested.top),static_cast<int>(target.bottom-target.top));
    const int x=std::clamp(static_cast<int>(requested.left),static_cast<int>(target.left),static_cast<int>(target.right)-w);
    const int y=std::clamp(static_cast<int>(requested.top),static_cast<int>(target.top),static_cast<int>(target.bottom)-h);
    result.rect={x,y,x+w,y+h};
    result.maximized=saved.maximized!=0;
    return result;
}
void persistGeometry(HWND hwnd) {
    if(!hwnd || IsIconic(hwnd)) return;
    WINDOWPLACEMENT placement{};placement.length=sizeof(placement);
    if(!GetWindowPlacement(hwnd,&placement)) return;
    RECT rect=IsZoomed(hwnd)?placement.rcNormalPosition:RECT{};
    if(!IsZoomed(hwnd) && !GetWindowRect(hwnd,&rect)) return;
    WindowGeometry data{};
    data.left=rect.left;data.top=rect.top;data.right=rect.right;data.bottom=rect.bottom;
    data.maximized=IsZoomed(hwnd)?1u:0u;
    HKEY key{};
    if(RegCreateKeyExW(HKEY_CURRENT_USER,PlacementKey,0,nullptr,0,KEY_SET_VALUE,
                       nullptr,&key,nullptr)==ERROR_SUCCESS) {
        RegSetValueExW(key,PlacementName,0,REG_BINARY,
                       reinterpret_cast<const BYTE*>(&data),static_cast<DWORD>(sizeof(data)));
        RegCloseKey(key);
    }
}
constexpr COLORREF Background=RGB(19,27,37);
constexpr COLORREF Bright=RGB(238,241,244);
constexpr COLORREF Muted=RGB(163,179,192);
enum ControlID {
    ID_START=101, ID_COUNT=102, ID_YEARS=103, ID_SEED=104,
    ID_PREVIOUS=110, ID_NEXT=111, ID_PLAY=112, ID_SLIDER=113, ID_EXPORT=114, ID_NEW=115,
    ID_PEOPLE=201, ID_DEAD=202
};

std::wstring number(int value) { return std::to_wstring(value); }
std::wstring yearLabel(int year) { return year<0?std::to_wstring(-year)+L" г. до начала летоисчисления":std::to_wstring(year)+L" г."; }
COLORREF rgb(const Rgb& c) { return RGB(c.r,c.g,c.b); }
Rgb terrainColor(Terrain t) {
    switch(t) {
        case Terrain::Sea: return {37,74,113};
        case Terrain::Plain: return {181,174,121};
        case Terrain::Forest: return {81,135,94};
        case Terrain::Jungle: return {41,107,86};
        case Terrain::Mountain: return {135,137,141};
    }
    return {0,0,0};
}
Rgb mix(const Rgb& a,const Rgb& b,int aPct) {
    const int bPct=100-aPct;
    return {static_cast<uint8_t>((int(a.r)*aPct+int(b.r)*bPct)/100),
            static_cast<uint8_t>((int(a.g)*aPct+int(b.g)*bPct)/100),
            static_cast<uint8_t>((int(a.b)*aPct+int(b.b)*bPct)/100)};
}
uint32_t dibPixel(Rgb c) { return (uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|c.b; }
std::wstring overview(Species type) {
    switch(type) {
    case Species::Human: return L"Многорасовые города, деревни и замки. Античные традиции, торговля, экспансия; рабство считается допустимым.";
    case Species::Elf: return L"Изолированные лесные поселения, осторожная дипломатия, защита природы и отрицание рабства.";
    case Species::Dwarf: return L"Горные чертоги, шахты, кузни; изоляционизм сочетается с вооружёнными вылазками за пленниками.";
    case Species::Outlaw: return L"Разрозненные шайки, пиратские стоянки, разбой и контрабандные картели; окрашенные области показывают зоны влияния.";
    case Species::Folk: return L"Маленькие скрытые общины лесных и сказочных существ; избегают встреч с крупными державами.";
    }
    return L"";
}
void textLine(HDC dc,int x,int y,const std::wstring& str,COLORREF color=Bright) {
    SetTextColor(dc,color);
    TextOutW(dc,x,y,str.c_str(),static_cast<int>(str.size()));
}
std::string toUtf8(std::wstring_view value) {
    if(value.empty()) return {};
    int len=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(len<=0) return {};
    std::string result(static_cast<size_t>(len),'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),len,nullptr,nullptr);
    return result;
}
bool writeUtf8(HANDLE file,const std::wstring& value) {
    std::string text=toUtf8(value);
    if(text.empty()) return true;
    DWORD written=0;
    return WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)!=0 && written==text.size();
}
}

class Application {
public:
    explicit Application(HINSTANCE instance):instance_(instance) {}
    ~Application() {
        if(font_) DeleteObject(font_);
        if(fontSmall_) DeleteObject(fontSmall_);
        if(fontTitle_) DeleteObject(fontTitle_);
        if(backgroundBrush_) DeleteObject(backgroundBrush_);
    }
    int run(int show);

private:
    HINSTANCE instance_{};
    HWND main_{}, details_{};
    HWND inputCount_{},inputYears_{},inputSeed_{},yearText_{},track_{},playButton_{};
    HWND detailsTitle_{},detailsHistory_{},detailsPeople_{},detailsBio_{},detailsShowDead_{};
    HFONT font_{},fontSmall_{},fontTitle_{};
    HBRUSH backgroundBrush_{};
    std::vector<HWND> setupControls_,runtimeControls_;
    World world_;
    MapCamera camera_;
    enum class DetailMode { Nation, Settlement };
    DetailMode detailMode_{DetailMode::Nation};
    bool started_{false},playing_{false};
    bool mapPressed_{false},mapDragged_{false};
    int dragStartX_{},dragStartY_{},dragLastX_{},dragLastY_{};
    int year_{0},selectedNation_{-1},selectedSettlement_{-1},selectedPerson_{-1};
    std::wstring hint_=L"Нажмите на цветную область карты, чтобы открыть её летопись.";

    static LRESULT CALLBACK mainProcedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK detailProcedure(HWND,UINT,WPARAM,LPARAM);
    LRESULT dispatchMain(HWND,UINT,WPARAM,LPARAM);
    LRESULT dispatchDetails(HWND,UINT,WPARAM,LPARAM);
    HWND createControl(HWND parent,const wchar_t* cls,const wchar_t* title,DWORD style,
                       int id,int x,int y,int w,int h,DWORD ex=0,HFONT customFont=nullptr);
    void onMainCreate();
    void createSetupControls();
    void createRuntimeControls();
    void layoutMain();
    RECT mapBounds() const;
    void paintMain(HDC);
    void paintSetup(HDC,const RECT&);
    void paintWorld(HDC,const RECT&);
    void paintLegend(HDC,const RECT&);
    void startGeneration();
    void showSetup();
    int readInput(HWND control,int fallback) const;
    void setYear(int year);
    void togglePlay();
    void clickMap(int x,int y);
    void openDetails(int nation);
    void openSettlement(int settlement);
    void layoutDetails();
    void updateDetails();
    void updateSettlementDetails();
    void populatePeopleList(bool settlementMode,int id);
    void updateBiography();
    void exportChronicle();
};

HWND Application::createControl(HWND parent,const wchar_t* cls,const wchar_t* title,DWORD style,
                                int id,int x,int y,int w,int h,DWORD ex,HFONT customFont) {
    HWND hwnd=CreateWindowExW(ex,cls,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
    if(hwnd) SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(customFont?customFont:font_),TRUE);
    return hwnd;
}
int Application::run(int show) {
    INITCOMMONCONTROLSEX icc{};icc.dwSize=sizeof(icc);icc.dwICC=ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    backgroundBrush_=CreateSolidBrush(Background);
    font_=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    fontSmall_=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    fontTitle_=CreateFontW(-31,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    WNDCLASSEXW mainClass{sizeof(WNDCLASSEXW)};
    mainClass.style=CS_HREDRAW|CS_VREDRAW;
    mainClass.lpfnWndProc=mainProcedure;
    mainClass.hInstance=instance_;
    mainClass.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    mainClass.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    mainClass.hbrBackground=backgroundBrush_;
    mainClass.lpszClassName=MainClass;
    RegisterClassExW(&mainClass);
    WNDCLASSEXW detailClass=mainClass;
    detailClass.lpfnWndProc=detailProcedure;
    detailClass.lpszClassName=DetailClass;
    RegisterClassExW(&detailClass);
    const auto geometry=startupGeometry();
    const RECT& rect=geometry.rect;
    main_=CreateWindowExW(0,MainClass,L"Фэнтези: летопись мира",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                          rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
                          nullptr,nullptr,instance_,this);
    if(!main_) return 1;
    ShowWindow(main_,geometry.maximized?SW_SHOWMAXIMIZED:show);UpdateWindow(main_);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
LRESULT CALLBACK Application::mainProcedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    Application* app=reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        app=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
        app->main_=hwnd;
    }
    return app?app->dispatchMain(hwnd,msg,w,l):DefWindowProcW(hwnd,msg,w,l);
}
LRESULT CALLBACK Application::detailProcedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    Application* app=reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        app=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
        app->details_=hwnd;
    }
    return app?app->dispatchDetails(hwnd,msg,w,l):DefWindowProcW(hwnd,msg,w,l);
}
void Application::onMainCreate() { createSetupControls();layoutMain(); }
void Application::createSetupControls() {
    setupControls_.push_back(createControl(main_,L"STATIC",L"Число цивилизаций (1–28)",SS_LEFT,0,71,234,305,28));
    inputCount_=createControl(main_,L"EDIT",L"10",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,ID_COUNT,72,267,230,34,WS_EX_CLIENTEDGE);
    setupControls_.push_back(inputCount_);
    setupControls_.push_back(createControl(main_,L"STATIC",L"Продолжительность истории (1–400 лет)",SS_LEFT,0,71,324,385,28));
    inputYears_=createControl(main_,L"EDIT",L"200",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,ID_YEARS,72,357,230,34,WS_EX_CLIENTEDGE);
    setupControls_.push_back(inputYears_);
    setupControls_.push_back(createControl(main_,L"STATIC",L"Зерно генератора (для повторяемого мира)",SS_LEFT,0,71,414,400,28));
    inputSeed_=createControl(main_,L"EDIT",L"42",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,ID_SEED,72,447,230,34,WS_EX_CLIENTEDGE);
    setupControls_.push_back(inputSeed_);
    setupControls_.push_back(createControl(main_,L"BUTTON",L"Создать мир",BS_PUSHBUTTON|WS_TABSTOP,ID_START,72,519,235,45));
}
void Application::createRuntimeControls() {
    runtimeControls_.push_back(createControl(main_,L"BUTTON",L"◀ Год",BS_PUSHBUTTON,ID_PREVIOUS,22,700,90,40));
    runtimeControls_.push_back(createControl(main_,L"BUTTON",L"Год ▶",BS_PUSHBUTTON,ID_NEXT,120,700,90,40));
    playButton_=createControl(main_,L"BUTTON",L"▶ Пуск",BS_PUSHBUTTON,ID_PLAY,220,700,114,40);
    runtimeControls_.push_back(playButton_);
    track_=createControl(main_,TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,ID_SLIDER,340,700,500,40);
    runtimeControls_.push_back(track_);
    yearText_=createControl(main_,L"STATIC",L"Год: 0",SS_CENTER,0,840,700,170,33);
    runtimeControls_.push_back(yearText_);
    runtimeControls_.push_back(createControl(main_,L"BUTTON",L"Летопись в TXT",BS_PUSHBUTTON,ID_EXPORT,1010,700,160,40));
    runtimeControls_.push_back(createControl(main_,L"BUTTON",L"Новый мир",BS_PUSHBUTTON,ID_NEW,1175,700,140,40));
    SendMessageW(track_,TBM_SETRANGEMIN,FALSE,0);
    SendMessageW(track_,TBM_SETRANGEMAX,TRUE,world_.settings().years);
    SendMessageW(track_,TBM_SETPOS,TRUE,0);
    layoutMain();
}
int Application::readInput(HWND hwnd,int fallback) const {
    wchar_t buffer[64]{};
    GetWindowTextW(hwnd,buffer,64);
    wchar_t* end=nullptr;
    long result=wcstol(buffer,&end,10);
    if(end==buffer || result<0 || result>1000000000) return fallback;
    return static_cast<int>(result);
}
void Application::startGeneration() {
    Settings s;
    s.civilizations=std::clamp(readInput(inputCount_,10),1,28);
    s.years=std::clamp(readInput(inputYears_,200),1,400);
    s.seed=static_cast<uint32_t>(readInput(inputSeed_,42));
    HCURSOR old=SetCursor(LoadCursorW(nullptr,IDC_WAIT));
    EnableWindow(main_,FALSE);
    world_.generate(s);
    EnableWindow(main_,TRUE);SetCursor(old);
    for(HWND control:setupControls_) ShowWindow(control,SW_HIDE);
    if(runtimeControls_.empty()) createRuntimeControls();
    else {
        for(HWND control:runtimeControls_) ShowWindow(control,SW_SHOW);
        SendMessageW(track_,TBM_SETRANGEMAX,TRUE,s.years);
        SendMessageW(track_,TBM_SETPOS,TRUE,0);
    }
    started_=true;year_=0;selectedNation_=-1;selectedSettlement_=-1;selectedPerson_=-1;
    camera_.reset(s.width,s.height);
    camera_.setViewport({mapBounds().left,mapBounds().top,mapBounds().right,mapBounds().bottom});
    SetWindowTextW(yearText_,L"Год: 0");
    hint_=L"Щелчок по точке — поселение, по области — государство.";
    layoutMain();InvalidateRect(main_,nullptr,TRUE);
}
void Application::showSetup() {
    if(playing_) togglePlay();
    if(mapPressed_ && GetCapture()==main_) ReleaseCapture();
    mapPressed_=false;mapDragged_=false;
    if(details_) DestroyWindow(details_);
    for(HWND control:runtimeControls_) ShowWindow(control,SW_HIDE);
    for(HWND control:setupControls_) ShowWindow(control,SW_SHOW);
    started_=false;
    InvalidateRect(main_,nullptr,TRUE);
}
RECT Application::mapBounds() const {
    RECT client{};GetClientRect(main_,&client);
    return RECT{24,88,std::max(225,static_cast<int>(client.right)-326),
                std::max(248,static_cast<int>(client.bottom)-153)};
}

void Application::layoutMain() {
    if(!main_) return;
    RECT rc{};GetClientRect(main_,&rc);
    if(!runtimeControls_.empty()) {
        int bottom=std::max(480,static_cast<int>(rc.bottom)-77);
        int side=rc.right-15;
        MoveWindow(runtimeControls_[0],24,bottom,81,36,TRUE);
        MoveWindow(runtimeControls_[1],110,bottom,81,36,TRUE);
        MoveWindow(playButton_,197,bottom,104,36,TRUE);
        int exportX=std::max(560,side-314);
        int yearX=std::max(450,exportX-132);
        int sliderW=std::max(80,yearX-315);
        MoveWindow(track_,307,bottom-1,sliderW,40,TRUE);
        MoveWindow(yearText_,yearX,bottom+5,127,29,TRUE);
        MoveWindow(runtimeControls_[5],exportX,bottom,157,36,TRUE);
        MoveWindow(runtimeControls_[6],exportX+163,bottom,140,36,TRUE);
    }
    if(!started_ && setupControls_.size()==7) {
        // Keep the start button visible on laptop displays with an 80%-height window.
        const int top=std::clamp(static_cast<int>(rc.bottom)/3+40,210,250);
        const int spacing=std::clamp((static_cast<int>(rc.bottom)-top-85)/3,66,90);
        for(int i=0;i<3;++i) {
            MoveWindow(setupControls_[static_cast<size_t>(i*2)],71,top+i*spacing,430,28,TRUE);
            MoveWindow(setupControls_[static_cast<size_t>(i*2+1)],72,top+i*spacing+30,230,32,TRUE);
        }
        MoveWindow(setupControls_[6],72,top+spacing*3+2,235,43,TRUE);
    }
    if(started_) {
        RECT map=mapBounds();
        camera_.setViewport({map.left,map.top,map.right,map.bottom});
    }
    InvalidateRect(main_,nullptr,FALSE);
}
void Application::setYear(int year) {
    if(!started_) return;
    int value=std::clamp(year,0,world_.settings().years);
    if(value==year_) return;
    year_=value;
    SendMessageW(track_,TBM_SETPOS,TRUE,year_);
    SetWindowTextW(yearText_,(L"Год: "+number(year_)).c_str());
    updateDetails();
    InvalidateRect(main_,nullptr,FALSE);
    if(year_==world_.settings().years && playing_) togglePlay();
}
void Application::togglePlay() {
    playing_=!playing_;
    if(playing_) {
        if(year_==world_.settings().years) setYear(0);
        SetTimer(main_,1,190,nullptr);
        SetWindowTextW(playButton_,L"❚❚ Пауза");
    } else {
        KillTimer(main_,1);
        SetWindowTextW(playButton_,L"▶ Пуск");
    }
}
void Application::paintSetup(HDC dc,const RECT& bounds) {
    SelectObject(dc,fontTitle_);
    textLine(dc,70,73,L"ФЭНТЕЗИ · ЛЕТОПИСЬ МИРА");
    SelectObject(dc,font_);
    textLine(dc,71,138,L"Генерация географии, государств и истории каждого жителя",Muted);
    textLine(dc,71,170,L"Выберите начальные условия, затем изучайте изменения мира по годам.",Muted);
    int right=std::max(530,static_cast<int>(bounds.right)/2+20);
    SelectObject(dc,fontTitle_);
    textLine(dc,right,234,L"Пять культур");
    SelectObject(dc,font_);
    const std::wstring descriptions[]={L"Люди — города, торговля, империи и республики",
        L"Эльфы — рощи, монархии и оборона",
        L"Дварфы — шахты, кланы и набеги",
        L"Пираты — лагеря и криминальные союзы",
        L"Сказочный народец — тайные лесные общины"};
    for(int i=0;i<5;++i) textLine(dc,right,295+i*48,descriptions[i],Muted);
    SelectObject(dc,fontSmall_);
    if(bounds.bottom>=680)
        textLine(dc,71,static_cast<int>(bounds.bottom)-45,L"Модель хранит биографии и поколения. Население ограничено ради скорости симуляции.",Muted);
}
void Application::paintWorld(HDC dc,const RECT& viewport) {
    const Snapshot& snap=world_.at(year_);
    const int w=world_.settings().width,h=world_.settings().height;
    const MapRect rect=camera_.drawnRect();
    RECT bounds{rect.left,rect.top,rect.right,rect.bottom};
    int saved=SaveDC(dc);
    IntersectClipRect(dc,viewport.left,viewport.top,viewport.right,viewport.bottom);
    std::vector<uint32_t> pixels(static_cast<size_t>(w*h));
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const int idx=world_.index(x,y);
        auto base=terrainColor(world_.tiles()[static_cast<size_t>(idx)].terrain);
        const int owner=snap.owner[static_cast<size_t>(idx)];
        if(owner>=0) base=mix(world_.nations()[static_cast<size_t>(owner)].color,base,72);
        else if(world_.tiles()[static_cast<size_t>(idx)].terrain==Terrain::Sea) {
            int shine=(x*5+y*11+world_.tiles()[static_cast<size_t>(idx)].height)%15;
            base.b=static_cast<uint8_t>(std::min(220,int(base.b)+shine));
        }
        pixels[static_cast<size_t>(idx)]=dibPixel(base);
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w;
    info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    SetStretchBltMode(dc,COLORONCOLOR);
    StretchDIBits(dc,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,
                  0,0,w,h,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
    HPEN pen=CreatePen(PS_SOLID,2,RGB(24,34,40));
    HGDIOBJ oldPen=SelectObject(dc,pen);
    const double sx=double(bounds.right-bounds.left)/w,sy=double(bounds.bottom-bounds.top)/h;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        int idx=world_.index(x,y),owner=snap.owner[static_cast<size_t>(idx)];
        if(x+1<w && owner!=snap.owner[static_cast<size_t>(idx+1)]) {
            int xx=bounds.left+int(std::lround((x+1)*sx));
            int y0=bounds.top+int(std::lround(y*sy));
            int y1=bounds.top+int(std::lround((y+1)*sy));
            MoveToEx(dc,xx,y0,nullptr);LineTo(dc,xx,y1);
        }
        if(y+1<h && owner!=snap.owner[static_cast<size_t>(idx+w)]) {
            int yy=bounds.top+int(std::lround((y+1)*sy));
            int x0=bounds.left+int(std::lround(x*sx));
            int x1=bounds.left+int(std::lround((x+1)*sx));
            MoveToEx(dc,x0,yy,nullptr);LineTo(dc,x1,yy);
        }
    }
    SelectObject(dc,oldPen);DeleteObject(pen);
    // Markers remain anchored to the same map tiles while zooming or dragging.
    HBRUSH white=CreateSolidBrush(RGB(245,233,193));
    HBRUSH chosen=CreateSolidBrush(RGB(255,183,79));
    HGDIOBJ oldBrush=SelectObject(dc,white);
    SelectObject(dc,GetStockObject(BLACK_PEN));
    for(const Settlement& place:world_.settlements()) {
        if(place.founded>year_) continue;
        int x=bounds.left+int(std::lround((place.position.x+.5)*sx));
        int y=bounds.top+int(std::lround((place.position.y+.5)*sy));
        if(x<viewport.left-14 || x>viewport.right+14 || y<viewport.top-14 || y>viewport.bottom+14) continue;
        bool city=place.kind==SettlementKind::City||place.kind==SettlementKind::MountainCity;
        int r=(city?6:4)+(camera_.zoom()>2.0?1:0);
        SelectObject(dc,place.id==selectedSettlement_?chosen:white);
        Ellipse(dc,x-r,y-r,x+r+1,y+r+1);
    }
    SelectObject(dc,oldBrush);DeleteObject(white);DeleteObject(chosen);
    SelectObject(dc,fontSmall_);
    SetBkMode(dc,TRANSPARENT);
    for(const Nation& nation:world_.nations()) {
        if(nation.founded>year_ || (nation.vanished>=0 && year_>=nation.vanished)) continue;
        if(snap.owner[static_cast<size_t>(world_.index(nation.capital.x,nation.capital.y))]!=nation.id) continue;
        int x=bounds.left+int(std::lround((nation.capital.x+.5)*sx))+8;
        int y=bounds.top+int(std::lround((nation.capital.y+.5)*sy))-9;
        std::wstring title=nation.name;
        if(title.size()>19) title=title.substr(0,18)+L"…";
        textLine(dc,x+1,y+1,title,RGB(9,16,22));
        textLine(dc,x,y,title,RGB(255,247,219));
    }
    if(camera_.zoom()>=2.0) {
        for(const Settlement& place:world_.settlements()) {
            if(place.founded>year_) continue;
            int x=bounds.left+int(std::lround((place.position.x+.5)*sx))+8;
            int y=bounds.top+int(std::lround((place.position.y+.5)*sy))+6;
            if(x<viewport.left || x>viewport.right-25 || y<viewport.top || y>viewport.bottom-16) continue;
            std::wstring title=place.name;
            if(title.size()>23) title=title.substr(0,22)+L"…";
            textLine(dc,x+1,y+1,title,RGB(10,20,26));
            textLine(dc,x,y,title,RGB(255,247,220));
        }
    }
    RestoreDC(dc,saved);
    HPEN frame=CreatePen(PS_SOLID,1,RGB(109,137,154));
    HGDIOBJ oldFrame=SelectObject(dc,frame);
    HGDIOBJ emptyBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    Rectangle(dc,viewport.left-1,viewport.top-1,viewport.right+1,viewport.bottom+1);
    SelectObject(dc,emptyBrush);SelectObject(dc,oldFrame);DeleteObject(frame);
}
void Application::paintLegend(HDC dc,const RECT& map) {
    RECT window{};GetClientRect(main_,&window);
    int left=window.right-305;
    SelectObject(dc,fontTitle_);
    textLine(dc,24,28,L"МИР · " + number(year_) + L" ГОД");
    SelectObject(dc,fontSmall_);
    textLine(dc,24,65,L"Колесо — масштаб · зажать ЛКМ — двигать · щелчок по точке — поселение",Muted);
    textLine(dc,left,65,L"Масштаб: "+number(static_cast<int>(std::lround(camera_.zoom()*100)))+L"%",Muted);
    SelectObject(dc,font_);
    textLine(dc,left,96,L"Государства");
    const Snapshot& snap=world_.at(year_);
    int count=0,visible=0;
    const int maxVisible=std::max(5,static_cast<int>(map.bottom-map.top-40)/35);
    for(const Nation& n:world_.nations()) {
        if(n.founded>year_ || n.id>=static_cast<int>(snap.nationStates.size()) || snap.nationStates[static_cast<size_t>(n.id)].territory<=0) continue;
        ++count;
        if(visible>=maxVisible) continue;
        int y=135+visible*35;
        HBRUSH swatch=CreateSolidBrush(rgb(n.color));
        RECT swatchRect{left,y,left+13,y+18};FillRect(dc,&swatchRect,swatch);DeleteObject(swatch);
        SelectObject(dc,fontSmall_);
        std::wstring text=n.name;
        if(text.size()>30) text=text.substr(0,29)+L"…";
        textLine(dc,left+21,y-1,text,n.id==selectedNation_?Bright:Muted);
        textLine(dc,left+21,y+16,L"Жителей: "+number(snap.nationStates[static_cast<size_t>(n.id)].population),RGB(118,142,159));
        ++visible;
    }
    if(count>visible) textLine(dc,left,145+visible*35,L"И ещё "+number(count-visible)+L" государств…",Muted);
    SelectObject(dc,fontSmall_);
    textLine(dc,24,window.bottom-119,hint_,Muted);
}
void Application::paintMain(HDC dc) {
    RECT rc{};GetClientRect(main_,&rc);
    FillRect(dc,&rc,backgroundBrush_);
    SetBkMode(dc,TRANSPARENT);
    if(!started_) paintSetup(dc,rc);
    else { RECT map=mapBounds(); paintWorld(dc,map);paintLegend(dc,map); }
}
void Application::clickMap(int x,int y) {
    RECT viewport=mapBounds();
    if(x<viewport.left||x>=viewport.right||y<viewport.top||y>=viewport.bottom) return;
    const Snapshot& snap=world_.at(year_);
    // Settlement markers have precedence over the country's territory.
    // Pick the closest marker, using the exact drawing projection.
    int nearest=-1;
    double distanceSq=1.e20;
    for(const Settlement& town:world_.settlements()) {
        if(town.founded>year_) continue;
        auto screen=camera_.worldToScreen(town.position.x+.5,town.position.y+.5);
        const double dx=x-screen.x,dy=y-screen.y;
        const double d=dx*dx+dy*dy;
        const bool city=town.kind==SettlementKind::City||town.kind==SettlementKind::MountainCity;
        const double hit=city?11.:9.;
        if(d<=hit*hit && d<distanceSq) { nearest=town.id;distanceSq=d; }
    }
    if(nearest>=0) { openSettlement(nearest);return; }
    auto worldPoint=camera_.screenToWorld(x,y);
    const int tileX=static_cast<int>(std::floor(worldPoint.x));
    const int tileY=static_cast<int>(std::floor(worldPoint.y));
    if(!world_.inside(tileX,tileY)) return;
    int idx=world_.index(tileX,tileY);
    int owner=snap.owner[static_cast<size_t>(idx)];
    if(owner>=0) openDetails(owner);
    else {
        hint_=world_.tiles()[static_cast<size_t>(idx)].terrain==Terrain::Sea?
            L"Это море. Выберите государство или значок поселения.":
            L"Эта территория пока не принадлежит ни одной державе.";
        InvalidateRect(main_,nullptr,FALSE);
    }
}
void Application::openDetails(int nation) {
    if(!started_ || nation<0 || static_cast<size_t>(nation)>=world_.nations().size()) return;
    detailMode_=DetailMode::Nation;selectedNation_=nation;selectedSettlement_=-1;selectedPerson_=-1;
    if(!details_) {
        details_=CreateWindowExW(WS_EX_APPWINDOW,DetailClass,L"Летопись государства",WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT,CW_USEDEFAULT,1120,730,main_,nullptr,instance_,this);
        if(!details_) return;
    }
    updateDetails();
    ShowWindow(details_,SW_SHOW);SetForegroundWindow(details_);
    InvalidateRect(main_,nullptr,FALSE);
}
void Application::openSettlement(int settlement) {
    if(!started_ || settlement<0 || static_cast<size_t>(settlement)>=world_.settlements().size() ||
       world_.settlements()[static_cast<size_t>(settlement)].founded>year_) return;
    detailMode_=DetailMode::Settlement;selectedSettlement_=settlement;selectedPerson_=-1;
    selectedNation_=world_.at(year_).settlementStates[static_cast<size_t>(settlement)].owner;
    if(!details_) {
        details_=CreateWindowExW(WS_EX_APPWINDOW,DetailClass,L"Летопись поселения",WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT,CW_USEDEFAULT,1120,730,main_,nullptr,instance_,this);
        if(!details_) return;
    }
    updateDetails();
    ShowWindow(details_,SW_SHOW);SetForegroundWindow(details_);
    InvalidateRect(main_,nullptr,FALSE);
}
void Application::layoutDetails() {
    if(!details_) return;
    RECT rc{};GetClientRect(details_,&rc);
    int w=std::max(620,static_cast<int>(rc.right)),h=std::max(400,static_cast<int>(rc.bottom));
    int leftW=std::max(300,(w*57)/100),rightX=leftW+30,rightW=std::max(140,w-rightX-20);
    if(detailsTitle_) MoveWindow(detailsTitle_,18,15,w-30,47,TRUE);
    if(detailsHistory_) MoveWindow(detailsHistory_,18,76,leftW-30,h-92,TRUE);
    if(detailsShowDead_) MoveWindow(detailsShowDead_,rightX,78,rightW,26,TRUE);
    int listH=std::max(145,(h-130)*43/100);
    if(detailsPeople_) MoveWindow(detailsPeople_,rightX,108,rightW,listH,TRUE);
    if(detailsBio_) MoveWindow(detailsBio_,rightX,115+listH,rightW,std::max(85,h-134-listH),TRUE);
}
void Application::updateDetails() {
    if(detailMode_==DetailMode::Settlement) {updateSettlementDetails();return;}
    if(!details_ || selectedNation_<0 || !started_) return;
    const Nation& n=world_.nations()[static_cast<size_t>(selectedNation_)];
    const Snapshot& snap=world_.at(year_);
    bool exists=n.founded<=year_;
    const NationYear* stats=exists && static_cast<size_t>(n.id)<snap.nationStates.size()?&snap.nationStates[static_cast<size_t>(n.id)]:nullptr;
    std::wstring title=n.name+L" — "+number(year_)+L" год";
    SetWindowTextW(details_,title.c_str());
    SetWindowTextW(detailsTitle_,title.c_str());
    std::wstring record=L"КУЛЬТУРА: "+std::wstring(speciesName(n.culture))+L"\r\n"+
        overview(n.culture)+L"\r\n\r\n";
    record+=L"ОСНОВАНО: "+yearLabel(n.founded)+L"\r\n";
    if(!exists) record+=L"В этот год государство ещё не существовало.\r\n";
    else if(stats) {
        record+=L"ФОРМА ПРАВЛЕНИЯ: "+std::wstring(governmentName(stats->government))+L"\r\n";
        record+=L"ЖИТЕЛЕЙ: "+number(stats->population)+L" (все учтены как отдельные персонажи)\r\n";
        record+=L"ТЕРРИТОРИЯ: "+number(stats->territory)+L" клеток\r\n";
        record+=L"ПОСЕЛЕНИЙ: "+number(stats->settlements)+L"\r\n";
        record+=L"КАЗНА: "+number(stats->treasury)+L"  |  СТАБИЛЬНОСТЬ: "+number(stats->stability)+L"\r\n";
        if(stats->ruler>=0 && static_cast<size_t>(stats->ruler)<world_.people().size()) {
            const auto& ruler=world_.people()[static_cast<size_t>(stats->ruler)];
            record+=L"ПРАВИТЕЛЬ: "+ruler.name+L"\r\n";
            if(ruler.family>=0) record+=L"РОД ПРАВИТЕЛЯ: "+
                world_.families()[static_cast<size_t>(ruler.family)].name+L"\r\n";
        }
        if(n.vanished>=0 && year_>=n.vanished) record+=L"СТАТУС: государство распалось в "+number(n.vanished)+L" г.\r\n";
        record+=L"\r\nПОСЕЛЕНИЯ\r\n";
        int listed=0;
        for(const auto& place:world_.settlements()) {
            if(place.founded>year_ || snap.owner[static_cast<size_t>(world_.index(place.position.x,place.position.y))]!=n.id) continue;
            record+=L"• "+place.name+L" ("+settlementName(place.kind)+L")\r\n";
            if(++listed>=26) { record+=L"...\r\n";break; }
        }
        if(!listed) record+=L"Поселения не сохранились.\r\n";
        record+=L"\r\nГИЛЬДИИ, КАРТЕЛИ, ОБЪЕДИНЕНИЯ\r\n";
        listed=0;
        for(const auto& guild:world_.guilds()) if(guild.nation==n.id && guild.founded<=year_) {
            record+=L"• "+guild.name+L": "+guild.purpose+L"\r\n";
            if(++listed>=18) {record+=L"...\r\n";break;}
        }
        if(!listed) record+=L"Сведений нет.\r\n";
    }
    record+=L"\r\nЛЕТОПИСЬ СОБЫТИЙ (новые сверху)\r\n";
    int shown=0;
    const auto& events=world_.events();
    for(auto it=events.rbegin();it!=events.rend();++it) {
        if(it->year>year_ || it->nation!=n.id) continue;
        record+=L"["+number(it->year)+L"] "+it->text+L"\r\n";
        if(++shown>=600) { record+=L"(Более ранние записи скрыты в окне; полный список доступен при экспорте.)\r\n";break; }
    }
    if(!shown) record+=L"Пока нет записей.\r\n";
    SendMessageW(detailsHistory_,EM_SETLIMITTEXT,1000000,0);
    SetWindowTextW(detailsHistory_,record.c_str());
    SendMessageW(detailsHistory_,EM_SETSEL,0,0);
    SendMessageW(detailsHistory_,EM_SCROLLCARET,0,0);
    populatePeopleList(false,n.id);
}
void Application::updateSettlementDetails() {
    if(!started_ || !details_ || selectedSettlement_<0 ||
       static_cast<size_t>(selectedSettlement_)>=world_.settlements().size()) return;
    const auto& town=world_.settlements()[static_cast<size_t>(selectedSettlement_)];
    const auto& snap=world_.at(year_);
    const bool exists=town.founded<=year_;
    std::wstring title=town.name+L" — "+number(year_)+L" год";
    SetWindowTextW(details_,title.c_str());
    SetWindowTextW(detailsTitle_,title.c_str());
    std::wstring record=L"ТИП: "+std::wstring(settlementName(town.kind))+L"\r\n";
    record+=L"МЕСТНОСТЬ: "+std::wstring(terrainName(world_.tiles()[static_cast<size_t>(
                   world_.index(town.position.x,town.position.y))].terrain))+L"\r\n";
    record+=L"КООРДИНАТЫ: "+number(town.position.x)+L", "+number(town.position.y)+L"\r\n";
    record+=L"ОСНОВАНО: "+yearLabel(town.founded)+L"\r\n";
    if(town.foundingNation>=0)
        record+=L"ОСНОВАТЕЛЬ: "+world_.nations()[static_cast<size_t>(town.foundingNation)].name+L"\r\n";
    if(!exists) {
        record+=L"\r\nВ этот год поселение ещё не было основано.\r\n";
    } else {
        const auto& state=snap.settlementStates[static_cast<size_t>(town.id)];
        record+=L"ВЛАДЕЛЕЦ: "+(state.owner>=0?
            world_.nations()[static_cast<size_t>(state.owner)].name:
            std::wstring(L"Независимая территория"))+L"\r\n";
        record+=L"БЛАГОСОСТОЯНИЕ: "+number(state.prosperity)+L"/100\r\n";
        record+=L"УЧТЁННЫХ УРОЖЕНЦЕВ У НЫНЕШНЕГО ВЛАДЕЛЬЦА: "+number(state.knownResidents)+L"\r\n";
        record+=L"(Учтены именные персонажи по месту рождения и принадлежности к державе.)\r\n";
        record+=L"\r\nМЕСТНАЯ ЛЕТОПИСЬ (новые сверху)\r\n";
        int shown=0;
        const auto& events=world_.settlementEvents();
        for(auto it=events.rbegin();it!=events.rend();++it) {
            if(it->year>year_ || it->settlement!=town.id) continue;
            record+=L"["+number(it->year)+L"] "+it->text+L"\r\n";
            if(++shown>=600) {record+=L"(Более ранние записи доступны при экспорте.)\r\n";break;}
        }
        if(!shown) record+=L"Пока нет событий.\r\n";
    }
    SendMessageW(detailsHistory_,EM_SETLIMITTEXT,1000000,0);
    SetWindowTextW(detailsHistory_,record.c_str());
    SendMessageW(detailsHistory_,EM_SETSEL,0,0);
    SendMessageW(detailsHistory_,EM_SCROLLCARET,0,0);
    populatePeopleList(true,town.id);
}
void Application::populatePeopleList(bool settlementMode,int id) {
    if(!detailsPeople_) return;
    // The list contains natives for settlements and members for countries.
    const bool includeDead=SendMessageW(detailsShowDead_,BM_GETCHECK,0,0)==BST_CHECKED;
    int previous=selectedPerson_;
    SendMessageW(detailsPeople_,LB_RESETCONTENT,0,0);
    int selectedRow=-1,rows=0;
    const auto& people=world_.people();
    for(auto it=people.rbegin();it!=people.rend();++it) {
        const Person& p=*it;
        if(p.born>year_) continue;
        bool alive=world_.aliveAt(p,year_);
        if(!includeDead && !alive) continue;
        if(settlementMode) {
            if(p.homeSettlement!=id) continue;
        } else {
            int relevantYear=alive?year_:std::max(p.born,p.died-1);
            if(world_.nationAt(p,relevantYear)!=id) continue;
        }
        int relevantYear=alive?year_:std::max(p.born,p.died-1);
        const auto& stage=world_.stageAt(p,relevantYear);
        std::wstring rowText=(alive?L"":L"† ")+p.name+L" — "+professionName(stage.profession);
        LRESULT row=SendMessageW(detailsPeople_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(rowText.c_str()));
        if(row!=LB_ERR && row!=LB_ERRSPACE) {
            SendMessageW(detailsPeople_,LB_SETITEMDATA,static_cast<WPARAM>(row),static_cast<LPARAM>(p.id));
            if(p.id==previous) selectedRow=static_cast<int>(row);
            if(++rows>=600) break;
        }
    }
    if(selectedRow<0 && rows>0) selectedRow=0;
    if(selectedRow>=0) {
        SendMessageW(detailsPeople_,LB_SETCURSEL,static_cast<WPARAM>(selectedRow),0);
        selectedPerson_=static_cast<int>(SendMessageW(detailsPeople_,LB_GETITEMDATA,static_cast<WPARAM>(selectedRow),0));
    } else selectedPerson_=-1;
    updateBiography();
}

void Application::updateBiography() {
    if(!detailsBio_) return;
    if(selectedPerson_<0 || static_cast<size_t>(selectedPerson_)>=world_.people().size()) {
        SetWindowTextW(detailsBio_,L"Здесь появится личная история выбранного жителя.");return;
    }
    const auto& p=world_.people()[static_cast<size_t>(selectedPerson_)];
    int relevantYear=std::min(year_,p.died==std::numeric_limits<int>::max()?year_:p.died-1);
    const auto& stage=world_.stageAt(p,relevantYear);
    std::wstring biography=p.name+L"\r\nРаса: "+speciesName(p.species)+L"\r\nРождение: "+yearLabel(p.born)+L"\r\n";
    if(p.died<=year_) biography+=L"Смерть: "+yearLabel(p.died)+L"\r\n";
    biography+=L"Профессия: "+std::wstring(professionName(stage.profession))+L"\r\n";
    biography+=L"Положение: "+std::wstring(statusName(stage.status))+L"\r\n";
    const auto& family=world_.families();
    const auto& everyone=world_.people();
    biography+=L"\r\nРОДОСЛОВНАЯ И СЕМЬЯ\r\n";
    if(p.family>=0 && static_cast<size_t>(p.family)<family.size())
        biography+=L"Род: "+family[static_cast<size_t>(p.family)].name+L"\r\n";
    auto personName=[&](int id) { return id>=0 && static_cast<size_t>(id)<everyone.size()?
        everyone[static_cast<size_t>(id)].name:std::wstring(L"неизвестно"); };
    biography+=L"Родитель 1: "+personName(p.parentA)+L"\r\n";
    biography+=L"Родитель 2: "+personName(p.parentB)+L"\r\n";
    const int spouse=world_.spouseAt(p,relevantYear);
    biography+=L"Супруг/супруга: "+(spouse>=0?personName(spouse):L"нет в выбранный год")+L"\r\n";
    biography+=L"Имущество: "+number(world_.wealthAt(p,relevantYear))+L" монет\r\n";
    biography+=L"\r\nПРЕДКИ (до трёх поколений)\r\n";
    auto ancestors=[&](auto&& self,int id,int generation)->void {
        if(id<0 || generation>3 || static_cast<size_t>(id)>=everyone.size()) return;
        const auto& ancestor=everyone[static_cast<size_t>(id)];
        biography+=std::wstring(static_cast<size_t>(generation)*2,L' ')+L"• "+ancestor.name+
                   L" (рожд. "+number(ancestor.born)+L")\r\n";
        self(self,ancestor.parentA,generation+1);
        self(self,ancestor.parentB,generation+1);
    };
    ancestors(ancestors,p.parentA,1);
    ancestors(ancestors,p.parentB,1);
    if(p.parentA<0 && p.parentB<0) biography+=L"Сведения о старших поколениях не сохранились.\r\n";
    biography+=L"\r\nДЕТИ\r\n";
    int visibleChildren=0;
    for(int child:p.children) if(everyone[static_cast<size_t>(child)].born<=year_) {
        const auto& descendant=everyone[static_cast<size_t>(child)];
        biography+=L"• "+descendant.name+L" ("+number(descendant.born)+L" г.)\r\n";
        ++visibleChildren;
    }
    if(!visibleChildren) biography+=L"Нет записей на выбранный год.\r\n";
    if(stage.guild>=0 && static_cast<size_t>(stage.guild)<world_.guilds().size()) biography+=L"Объединение: "+world_.guilds()[static_cast<size_t>(stage.guild)].name+L"\r\n";
    if(p.homeSettlement>=0 && static_cast<size_t>(p.homeSettlement)<world_.settlements().size()) biography+=L"Родное поселение: "+world_.settlements()[static_cast<size_t>(p.homeSettlement)].name+L"\r\n";
    // Show the character exactly as they were in the selected year, not at
    // the end of the simulation.  Dead residents retain their last known stats.
    const auto& stats=world_.statsAt(p,relevantYear);
    biography+=L"\r\nРАЗВИТИЕ ЛИЧНОСТИ — "+yearLabel(relevantYear)+L"\r\n";
    biography+=L"Значения: 0–100; в скобках — начальный уровень.\r\n";
    biography+=L"\r\nСПОСОБНОСТИ\r\n";
    for(size_t i=0;i<static_cast<size_t>(Ability::Count);++i) {
        biography+=std::wstring(abilityName(static_cast<Ability>(i)))+L": "+
            number(stats.abilities[i])+L" (было "+number(p.innate.abilities[i])+L")\r\n";
    }
    biography+=L"\r\nНАВЫКИ\r\n";
    for(size_t i=0;i<static_cast<size_t>(Skill::Count);++i) {
        biography+=std::wstring(skillName(static_cast<Skill>(i)))+L": "+
            number(stats.skills[i])+L" (было "+number(p.innate.skills[i])+L")\r\n";
    }
    biography+=L"\r\nЧЕРТЫ ХАРАКТЕРА\r\n";
    for(size_t i=0;i<static_cast<size_t>(Personality::Count);++i) {
        biography+=std::wstring(personalityName(static_cast<Personality>(i)))+L": "+
            number(stats.traits[i])+L" (было "+number(p.innate.traits[i])+L")\r\n";
    }
    biography+=L"\r\nБИОГРАФИЯ\r\n";
    for(const auto& note:p.biography) if(note.year<=year_) biography+=L"["+number(note.year)+L"] "+note.text+L"\r\n";
    SetWindowTextW(detailsBio_,biography.c_str());
    SendMessageW(detailsBio_,EM_SETSEL,0,0);
    SendMessageW(detailsBio_,EM_SCROLLCARET,0,0);
}
void Application::exportChronicle() {
    if(!started_) return;
    wchar_t path[MAX_PATH]=L"fantasy_chronicle.txt";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=main_;
    dialog.lpstrFilter=L"Текстовый файл (*.txt)\0*.txt\0Все файлы (*.*)\0*.*\0";
    dialog.lpstrFile=path;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"txt";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(!GetSaveFileNameW(&dialog)) return;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) { MessageBoxW(main_,L"Не удалось открыть файл для записи.",L"Ошибка",MB_OK|MB_ICONERROR);return; }
    const char bom[]{static_cast<char>(0xEF),static_cast<char>(0xBB),static_cast<char>(0xBF)};
    DWORD written{};WriteFile(file,bom,3,&written,nullptr);
    bool ok=true;
    auto output=[&](const std::wstring& s){ if(ok) ok=writeUtf8(file,s); };
    output(L"ЛЕТОПИСЬ ФЭНТЕЗИЙНОГО МИРА\r\nЗерно: "+number(static_cast<int>(world_.settings().seed))+L". Период: 0–"+number(world_.settings().years)+L" гг.\r\n\r\n");
    output(L"=== ГОСУДАРСТВА ===\r\n");
    for(const auto& n:world_.nations()) {
        output(n.name+L" | "+speciesName(n.culture)+L" | основано: "+number(n.founded)+
               L" | конец: "+(n.vanished>=0?number(n.vanished):L"не наступил")+L"\r\n");
    }
    output(L"\r\n=== ИСТОРИЧЕСКИЕ СОБЫТИЯ ===\r\n");
    for(const auto& event:world_.events()) {
        const std::wstring nation=event.nation>=0?world_.nations()[static_cast<size_t>(event.nation)].name:L"Мир";
        output(L"["+number(event.year)+L"] ["+nation+L"] "+event.text+L"\r\n");
    }
    output(L"\r\n=== ПОСЕЛЕНИЯ И МЕСТНАЯ ИСТОРИЯ ===\r\n");
    for(const auto& town:world_.settlements()) {
        output(L"\r\n#"+number(town.id)+L" "+town.name+L" ("+settlementName(town.kind)+
               L", основано: "+number(town.founded)+L")\r\n");
        for(const auto& event:world_.settlementEvents()) if(event.settlement==town.id)
            output(L"  ["+number(event.year)+L"] "+event.text+L"\r\n");
    }
    output(L"\r\n=== РОДЫ И ДИНАСТИИ ===\r\n");
    for(const auto& family:world_.families())
        output(L"#"+number(family.id)+L" "+family.name+L" | основатель: "+
               world_.people()[static_cast<size_t>(family.founder)].name+L"\r\n");
    output(L"\r\n=== БРАКИ И НАСЛЕДСТВА ===\r\n");
    for(const auto& marriage:world_.marriages())
        output(L"["+number(marriage.year)+L"] Брак: "+world_.people()[static_cast<size_t>(marriage.first)].name+
               L" и "+world_.people()[static_cast<size_t>(marriage.second)].name+L"\r\n");
    for(const auto& inheritance:world_.inheritances())
        output(L"["+number(inheritance.year)+L"] Наследство: "+
               world_.people()[static_cast<size_t>(inheritance.from)].name+L" → "+
               world_.people()[static_cast<size_t>(inheritance.to)].name+L" ("+number(inheritance.wealth)+L" монет)\r\n");
    output(L"\r\n=== ИМЕННЫЕ ЖИТЕЛИ И БИОГРАФИИ ===\r\n");
    for(const auto& p:world_.people()) {
        output(L"\r\n#"+number(p.id)+L" "+p.name+L" ("+speciesName(p.species)+L", "+number(p.born)+L"–"+
               (p.died==std::numeric_limits<int>::max()?L"жив":number(p.died))+L")\r\n");
        const auto& stats=world_.statsAt(p,p.died==std::numeric_limits<int>::max()?world_.settings().years:p.died-1);
        output(L"  Род: "+(p.family>=0?world_.families()[static_cast<size_t>(p.family)].name:L"неизвестен")+
               L" | родители: "+(p.parentA>=0?world_.people()[static_cast<size_t>(p.parentA)].name:L"неизвестен")+
               L", "+(p.parentB>=0?world_.people()[static_cast<size_t>(p.parentB)].name:L"неизвестен")+L"\r\n");
        output(L"  Имущество на последнем прожитом году: "+number(world_.wealthAt(p,world_.settings().years))+L" монет\r\n");
        output(L"  Итоговые способности (0–100): ");
        for(size_t i=0;i<static_cast<size_t>(Ability::Count);++i)
            output(std::wstring(abilityName(static_cast<Ability>(i)))+L"="+number(stats.abilities[i])+L"; ");
        output(L"\r\n  Итоговые навыки: ");
        for(size_t i=0;i<static_cast<size_t>(Skill::Count);++i)
            output(std::wstring(skillName(static_cast<Skill>(i)))+L"="+number(stats.skills[i])+L"; ");
        output(L"\r\n  Черты характера: ");
        for(size_t i=0;i<static_cast<size_t>(Personality::Count);++i)
            output(std::wstring(personalityName(static_cast<Personality>(i)))+L"="+number(stats.traits[i])+L"; ");
        output(L"\r\n");
        for(const auto& note:p.biography) output(L"  ["+number(note.year)+L"] "+note.text+L"\r\n");
    }
    CloseHandle(file);
    if(!ok) MessageBoxW(main_,L"Ошибка записи. Файл может быть неполным.",L"Ошибка",MB_OK|MB_ICONERROR);
    else MessageBoxW(main_,L"Летопись, события и биографии сохранены в UTF-8.",L"Экспорт завершён",MB_OK|MB_ICONINFORMATION);
}
LRESULT Application::dispatchMain(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: onMainCreate();return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(l);
        const RECT work=workingAreaFor();
        info->ptMinTrackSize.x=std::min(1050,static_cast<int>(work.right-work.left)*4/5);
        info->ptMinTrackSize.y=std::min(680,static_cast<int>(work.bottom-work.top)*4/5);
        return 0;
    }
    case WM_SIZE: layoutMain();return 0;
    case WM_EXITSIZEMOVE: persistGeometry(hwnd);return 0;
    case WM_CLOSE: persistGeometry(hwnd);DestroyWindow(hwnd);return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(w),Bright);
        SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    case WM_PAINT: { PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);paintMain(dc);EndPaint(hwnd,&ps);return 0; }
    case WM_LBUTTONDOWN:
        if(started_) {
            RECT viewport=mapBounds();
            int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
            if(x>=viewport.left && x<viewport.right && y>=viewport.top && y<viewport.bottom) {
                mapPressed_=true;mapDragged_=false;
                dragStartX_=dragLastX_=x;dragStartY_=dragLastY_=y;
                SetCapture(hwnd);return 0;
            }
        }
        break;
    case WM_MOUSEMOVE:
        if(mapPressed_ && GetCapture()==hwnd) {
            int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
            if(!mapDragged_ && (std::abs(x-dragStartX_)>4 || std::abs(y-dragStartY_)>4))
                mapDragged_=true;
            if(mapDragged_) {
                camera_.panPixels(x-dragLastX_,y-dragLastY_);
                InvalidateRect(hwnd,nullptr,FALSE);
            }
            dragLastX_=x;dragLastY_=y;
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if(mapPressed_) {
            const bool isClick=!mapDragged_;
            mapPressed_=false;mapDragged_=false;
            if(GetCapture()==hwnd) ReleaseCapture();
            if(isClick) clickMap(GET_X_LPARAM(l),GET_Y_LPARAM(l));
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        mapPressed_=false;mapDragged_=false;return 0;
    case WM_MOUSEWHEEL:
        if(started_) {
            POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            ScreenToClient(hwnd,&point);
            RECT viewport=mapBounds();
            if(point.x>=viewport.left && point.x<viewport.right && point.y>=viewport.top && point.y<viewport.bottom) {
                double steps=double(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA;
                camera_.zoomAt(point.x,point.y,std::pow(1.2,steps));
                InvalidateRect(hwnd,nullptr,FALSE);
                return 0;
            }
        }
        break;
    case WM_SETCURSOR:
        if(started_ && LOWORD(l)==HTCLIENT) {
            POINT point{};GetCursorPos(&point);ScreenToClient(hwnd,&point);
            RECT viewport=mapBounds();
            if(point.x>=viewport.left && point.x<viewport.right && point.y>=viewport.top && point.y<viewport.bottom) {
                SetCursor(LoadCursorW(nullptr,mapPressed_?IDC_SIZEALL:IDC_HAND));return TRUE;
            }
        }
        break;
    case WM_COMMAND:
        switch(LOWORD(w)) {
            case ID_START: startGeneration();return 0;
            case ID_PREVIOUS: setYear(year_-1);return 0;
            case ID_NEXT: setYear(year_+1);return 0;
            case ID_PLAY: togglePlay();return 0;
            case ID_EXPORT: exportChronicle();return 0;
            case ID_NEW: showSetup();return 0;
        }
        break;
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(l)==track_) setYear(static_cast<int>(SendMessageW(track_,TBM_GETPOS,0,0)));
        return 0;
    case WM_TIMER: if(w==1 && playing_) setYear(year_+1);return 0;
    case WM_KEYDOWN:
        if(started_) {
            if(w==VK_LEFT) {setYear(year_-1);return 0;}
            if(w==VK_RIGHT) {setYear(year_+1);return 0;}
            if(w==VK_SPACE) {togglePlay();return 0;}
        }
        break;
    case WM_DESTROY:
        if(playing_) KillTimer(hwnd,1);
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
LRESULT Application::dispatchDetails(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE:
        detailsTitle_=createControl(hwnd,L"STATIC",L"История",SS_LEFT,0,18,15,700,45,0,fontTitle_);
        detailsHistory_=createControl(hwnd,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
                                      0,18,76,600,570,WS_EX_CLIENTEDGE,fontSmall_);
        detailsShowDead_=createControl(hwnd,L"BUTTON",L"Включить умерших жителей",BS_AUTOCHECKBOX,
                                        ID_DEAD,650,78,310,27,0,fontSmall_);
        detailsPeople_=createControl(hwnd,L"LISTBOX",L"",WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
                                      ID_PEOPLE,650,108,400,250,WS_EX_CLIENTEDGE,fontSmall_);
        detailsBio_=createControl(hwnd,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
                                   0,650,370,400,240,WS_EX_CLIENTEDGE,fontSmall_);
        SendMessageW(detailsBio_,EM_SETLIMITTEXT,1000000,0);
        layoutDetails();return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(l);
        info->ptMinTrackSize.x=840;info->ptMinTrackSize.y=560;
        return 0;
    }
    case WM_SIZE:layoutDetails();return 0;
    case WM_COMMAND:
        if(LOWORD(w)==ID_PEOPLE && HIWORD(w)==LBN_SELCHANGE) {
            LRESULT selection=SendMessageW(detailsPeople_,LB_GETCURSEL,0,0);
            if(selection!=LB_ERR) { selectedPerson_=static_cast<int>(SendMessageW(detailsPeople_,LB_GETITEMDATA,static_cast<WPARAM>(selection),0));updateBiography(); }
            return 0;
        }
        if(LOWORD(w)==ID_DEAD && HIWORD(w)==BN_CLICKED) {updateDetails();return 0;}
        break;
    case WM_ERASEBKGND:return 1;
    case WM_CTLCOLORSTATIC: {
        HDC dc=reinterpret_cast<HDC>(w);
        HWND control=reinterpret_cast<HWND>(l);
        SetTextColor(dc,Bright);
        SetBkColor(dc,Background);
        // Read-only EDIT windows send WM_CTLCOLORSTATIC too.  Transparent text
        // leaves old glyphs on screen when the multi-line control scrolls.
        bool isEdit=control==detailsHistory_ || control==detailsBio_;
        SetBkMode(dc,isEdit?OPAQUE:TRANSPARENT);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        RECT rc{};GetClientRect(hwnd,&rc);FillRect(dc,&rc,backgroundBrush_);
        EndPaint(hwnd,&ps);return 0;
    }
    case WM_DESTROY:
        details_=nullptr;detailsTitle_=nullptr;detailsHistory_=nullptr;detailsPeople_=nullptr;detailsBio_=nullptr;detailsShowDead_=nullptr;
        selectedPerson_=-1;selectedSettlement_=-1;
        if(main_) InvalidateRect(main_,nullptr,FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    Application app(instance);
    return app.run(show);
}
