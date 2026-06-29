# Plan Hoàn Chỉnh V2 — Ubuntu System Manager Dashboard (Qt6)

> **Phiên bản này thay thế hoàn toàn `PLAN_HOANCHINH.md` cũ.** Điểm khác biệt chính: bổ sung **Visual Layout Spec chi tiết** (section 4A) với ASCII wireframe annotated, pixel/spacing cụ thể cho từng vùng, và quy tắc rõ ràng về cách đặt widget — đây là phần agent cần đọc trước khi code bất kỳ widget nào. Mọi quyết định logic/command/cấu trúc file từ bản cũ vẫn giữ nguyên.

## 0. Input bắt buộc

- [ ] `CODEBASE_SURVEY.md` — tên lệnh/tham số/output format thật của 3 lab, tra cứu khi code `CommandRunner`, không tự đoán.
- [ ] `style.qss` — đặt tại `04_qt_dashboard/resources/style.qss`, áp dụng nguyên, không tự thêm màu/font-size/radius mới ở `.cpp`.
- [ ] File plan này là nguồn sự thật DUY NHẤT về layout. Nếu bất kỳ quyết định nào mâu thuẫn với file cũ, file này thắng.

---

## 1. Phạm vi

Dashboard là lớp UI gọi `QProcess` vào 3 lab CLI có sẵn (Shell & Automation, Process & Network, Kernel Modules & Firewall). Không sửa logic 3 lab gốc.

---

## 2. Cấu trúc thư mục

```
04_qt_dashboard/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── MainWindow.h / .cpp
│   ├── components/
│   │   ├── Sidebar.h / .cpp
│   │   ├── HeaderBar.h / .cpp
│   │   ├── TabBar.h / .cpp
│   │   ├── CommandRunner.h / .cpp
│   │   ├── ProcessRegistry.h / .cpp
│   │   ├── ActivityLogger.h / .cpp
│   │   ├── FileExplorer.h / .cpp
│   │   └── SimpleFormDialog.h / .cpp
│   └── pages/
│       ├── ShellAutomationPage.h / .cpp
│       ├── ProcessNetworkPage.h / .cpp
│       ├── KernelModulesPage.h / .cpp
│       ├── DemoScenariosPage.h / .cpp
│       └── ActivityLogPage.h / .cpp
└── resources/
    └── style.qss
```

---

## 3. Theme — Ubuntu/Yaru token (không đổi)

Set trong `main.cpp` trước khi load QSS:
```cpp
QApplication::setStyle("Fusion");
QFont font("Ubuntu", 10);
if (!QFontInfo(font).exactMatch()) font.setFamily("Noto Sans");
app.setFont(font);
```

**Palette chính** (tất cả đã có trong `style.qss`, không khai báo lại ở `.cpp`):
- Cam primary: `#E95420`, hover `#C7401A`, pressed `#A8350F`
- Text chính: `#2C2C2C`, text phụ: `#6E6E6E`
- Card/nền widget: `#FFFFFF`, border: `#E0DFDE`, border-radius: `6px`
- Sidebar nền: `#F2F1F0`, tab bar nền: `#F2F1F0`
- Output panel: nền `#2C2C2C`, text `#E0E0E0`
- Selection highlight: `#FCE4D9`

---

## 4. Layout tổng thể — MainWindow

### 4A. Visual Layout Spec ← ĐỌC TRƯỚC KHI CODE BẤT KỲ WIDGET NÀO

Phần này mô tả **trực quan** từng vùng của app, theo đúng chuẩn GNOME/Ubuntu. Agent phải khớp wireframe này khi code.

#### 4A.1 Skeleton tổng thể (window 1280×800)

```
┌────────────────────────────────────────────────────────────────────────────┐
│ HEADER BAR (h=56, nền #FFFFFF, border-bottom 1px #E0DFDE)                  │
│  [icon 32×32]  Ubuntu System Manager      [badge Root/Limited]  [spacer]   │
│                font-size:20px bold #2C2C2C                                  │
├──────────────┬─────────────────────────────────────────────────────────────┤
│              │ CONTENT AREA (QStackedWidget, nền #FAFAFA, padding 0)       │
│   SIDEBAR    │                                                              │
│   (w=220)    │   ┌─ TAB BAR (segmented, h=44, nền #F2F1F0 radius 6px) ─┐  │
│   nền #F2F1F0│   │  [Files] [Cron Jobs] [System Time] [Packages]        │  │
│   border-r   │   └──────────────────────────────────────────────────────┘  │
│   1px #E0DFDE│                                                              │
│              │   ┌─ PAGE CONTENT (QSplitter vertical) ──────────────────┐  │
│  ▸ Shell &   │   │                                                        │  │
│    Automation│   │  ┌─ TOP AREA (stretch=5) ──────────────────────────┐ │  │
│  ▸ Process & │   │  │  ACTION BAR (h=44, nền trong suốt)               │ │  │
│    Network   │   │  │  [List] [More ▾]   ← trái; phải còn trống       │ │  │
│  ▸ Kernel    │   │  │                                                   │ │  │
│    Modules   │   │  │  MAIN WIDGET (file tree / table / form card)     │ │  │
│  ── ─ ─ ─ ──│   │  │  tự co giãn lấp đầy phần còn lại                │ │  │
│  ▸ Demo      │   │  └──────────────────────────────────────────────────┘ │  │
│    Scenarios │   │                                                        │  │
│  ▸ Activity  │   │  ┌─ OUTPUT PANEL (stretch=1, minH=80) ──────────────┐ │  │
│    Log       │   │  │  nền #2C2C2C radius 6px                           │ │  │
│              │   │  │  [label "Output"] [Clear ×]       ← header       │ │  │
│              │   │  │  QPlainTextEdit monospace text #E0E0E0             │ │  │
│              │   │  └──────────────────────────────────────────────────┘ │  │
│              │   └────────────────────────────────────────────────────────┘  │
└──────────────┴─────────────────────────────────────────────────────────────┘
```

#### 4A.2 Sidebar — chi tiết widget

```
┌─────────────────────────┐
│  SIDEBAR (w=220)         │
│  setMinimumWidth(180)    │
│  setMaximumWidth(320)    │
│  padding: 8px 0px        │
│                          │
│  ┌─────────────────────┐ │
│  │ QPushButton checkable│ │  ← mỗi nút chiếm full width
│  │ text-align: left     │ │
│  │ padding: 10px 16px   │ │  ← 10px top/bottom, 16px left/right
│  │ border-left 3px:     │ │
│  │   transparent (off)  │ │
│  │   #E95420 (checked)  │ │
│  │ font-size: 13px      │ │
│  │ height: auto         │ │  ← KHÔNG đặt fixed height, để text wrap
│  └─────────────────────┘ │
│                          │
│  [Shell & Automation]    │  ← checked → nền #FFFFFF, border-left cam
│  [Process & Network]     │
│  [Kernel Modules]        │
│  ─────────────────────   │  ← QFrame separator 1px #E0DFDE, margin 8px
│  [Demo Scenarios]        │
│  [Activity Log]          │
└─────────────────────────┘
```

**KHÔNG làm**:
- Không đặt icon lớn phía trên tên (tab-style dọc với icon)
- Không dùng `QListWidget` hay `QTreeWidget` cho sidebar
- Không đặt fixed height cho mỗi nút sidebar
- Không thêm section header text trong sidebar nếu plan không yêu cầu

#### 4A.3 Header Bar — chi tiết widget

```
┌──────────────────────────────────────────────────────────────────────────┐
│  HEADER BAR (h=56, padding: 0 16px)                                       │
│                                                                            │
│  [QLabel icon 32×32]  [QLabel "Ubuntu System Manager" #headerTitle]       │
│                        font-size:20px bold                                 │
│                                                                            │
│                                         [QLabel badge #badgeRoot]         │
│                                          "● Root mode"  hoặc              │
│                                         [QLabel badge #badgeLimited]      │
│                                          "⚠ Limited mode"                 │
└──────────────────────────────────────────────────────────────────────────┘
```

Layout code:
```cpp
// Dùng QHBoxLayout, KHÔNG dùng QGridLayout
QHBoxLayout* hlay = new QHBoxLayout(header);
hlay->setContentsMargins(16, 0, 16, 0);
hlay->addWidget(iconLabel);          // 32×32
hlay->addSpacing(10);
hlay->addWidget(titleLabel);         // #headerTitle
hlay->addStretch();                  // đẩy badge sang phải
hlay->addWidget(badgeLabel);         // #badgeRoot hoặc #badgeLimited
```

**KHÔNG làm**:
- Không thêm subtitle dòng 2 trong header chính (subtitle chỉ dùng nếu plan chỉ định rõ trang nào)
- Không thêm menu bar Qt mặc định (setMenuBar(nullptr))
- Không thêm toolbar Qt (không dùng QToolBar)

#### 4A.4 Tab Bar (Segmented Control) — chi tiết

```
┌──────────────────────────────────────────────────────────┐
│  #tabBar (nền #F2F1F0 radius 6px, padding: 4px)           │
│                                                            │
│  [Files ●] [Cron Jobs] [System Time] [Packages]           │
│     ↑                                                      │
│  checked: nền #FFFFFF radius 4px, text #E95420 bold       │
│  unchecked: transparent, text #6E6E6E                     │
│  hover: nền #E8E7E6                                       │
│  padding mỗi nút: 6px 14px                               │
└──────────────────────────────────────────────────────────┘
```

Layout code:
```cpp
// Tab bar nằm trong QVBoxLayout của page, TRƯỚC pageSplitter
// Tab bar KHÔNG nằm trong splitter — cố định phía trên
QWidget* tabBarWidget = new QWidget(this);
tabBarWidget->setObjectName("tabBar");
QHBoxLayout* tlay = new QHBoxLayout(tabBarWidget);
tlay->setContentsMargins(4, 4, 4, 4);
tlay->setSpacing(2);

QButtonGroup* tabGroup = new QButtonGroup(this);
for (auto& [id, label] : tabs) {
    QPushButton* btn = new QPushButton(label, tabBarWidget);
    btn->setObjectName("tabBtn");
    btn->setCheckable(true);
    tabGroup->addButton(btn, id);
    tlay->addWidget(btn);
}
tlay->addStretch();  // nút tab gom sang trái, không kéo full width
tabGroup->button(0)->setChecked(true);
```

**KHÔNG làm**:
- Không dùng `QTabWidget` mặc định (nó tạo ra tab style Qt, không giống Ubuntu)
- Không kéo từng nút tab thành full width (không addStretch giữa các nút)
- Không đặt chiều cao cố định cho tab bar widget > 52px

#### 4A.5 Action Bar — chi tiết (quan trọng nhất)

```
┌──────────────────────────────────────────────────────────┐
│  ACTION BAR (h=44, padding: 6px 0px)                      │
│                                                            │
│  [List]  [More ▾]        ← chỉ 2 widget, gom sang TRÁI   │
│    ↑         ↑                                             │
│  #actionBarBtn  QToolButton#moreBtn                        │
│  h=32, padding 0 14px    h=32, padding 0 12px             │
│                                                            │
│  addStretch() đẩy hết về phải → bên phải action bar rỗng │
└──────────────────────────────────────────────────────────┘
```

Layout code — **phải làm đúng như này**:
```cpp
QHBoxLayout* actionLay = new QHBoxLayout();
actionLay->setContentsMargins(0, 6, 0, 6);
actionLay->setSpacing(8);

QPushButton* listBtn = new QPushButton("List", this);
listBtn->setObjectName("actionBarBtn");

QToolButton* moreBtn = new QToolButton(this);
moreBtn->setObjectName("moreBtn");
moreBtn->setText("More ▾");
moreBtn->setPopupMode(QToolButton::InstantPopup);
// ... build QMenu và gắn vào moreBtn

actionLay->addWidget(listBtn);
actionLay->addWidget(moreBtn);
actionLay->addStretch();  // ← BẮT BUỘC: đẩy sang phải để nút gom trái
```

**KHÔNG làm**:
- Không dùng `setSizePolicy(Expanding, Fixed)` cho các nút action làm chúng kéo full width
- Không bỏ `addStretch()` cuối — nếu thiếu, nút sẽ dãn ra chiếm hết hàng
- Không xếp các nút action theo QVBoxLayout (không xếp dọc)
- Không thêm quá 2 nút bên ngoài More (tối đa: 1 primary action + 1 More button)

#### 4A.6 Output Panel — chi tiết

```
┌──────────────────────────────────────────────────────────┐
│  OUTPUT PANEL (#outputPanel, nền #2C2C2C, radius 6px)     │
│  margin: 0, padding: 0                                    │
│                                                            │
│  ┌────────────────────────────────────────────────────┐   │
│  │ HEADER ROW (h=32, padding: 4px 12px)               │   │
│  │ [QLabel "Output"]          [Show/Hide]  [× Clear]  │   │
│  │                             h=24         h=24       │   │
│  └────────────────────────────────────────────────────┘   │
│  ┌────────────────────────────────────────────────────┐   │
│  │ QPlainTextEdit  (ẩn mặc định, bấm Show mới hiện)    │   │
│  │ nền #2C2C2C, text #E0E0E0                           │   │
│  │ font: "Ubuntu Mono" 12px                            │   │
│  │ padding: 8px 12px                                   │   │
│  │ readOnly = true                                     │   │
│  │ lineWrapMode = NoWrap                               │   │
│  └────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

**Output panel mặc định HIDDEN — chỉ hiện header cho tới khi user bấm Show:**

```cpp
QWidget* outputPanel = new QWidget(this);
outputPanel->setObjectName("outputPanel");
outputPanel->setMinimumHeight(32);
outputPanel->setMaximumHeight(32);

QVBoxLayout* outLay = new QVBoxLayout(outputPanel);
outLay->setContentsMargins(0, 0, 0, 0);
outLay->setSpacing(0);

// Header row luôn visible
QWidget* outHeader = new QWidget(outputPanel);
outHeader->setFixedHeight(32);
// ... QLabel "Output" + QPushButton "Show/Hide" + QPushButton "× Clear"
outLay->addWidget(outHeader);

QPlainTextEdit* outputEdit = new QPlainTextEdit(outputPanel);
outputEdit->setReadOnly(true);
outputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
outputEdit->setVisible(false);   // HIDDEN mặc định
outLay->addWidget(outputEdit);   // stretch=1 khi visible

// Toggle logic
connect(toggleBtn, &QPushButton::clicked, this, [=]() mutable {
    const bool willHide = outputEdit->isVisible();
    outputEdit->setVisible(!willHide);
    toggleBtn->setText(willHide ? "Show" : "Hide");
    outputPanel->setMaximumHeight(willHide ? 32 : QWIDGETSIZE_MAX);
    outputPanel->setMinimumHeight(willHide ? 32 : 80);
});

connect(clearBtn, &QPushButton::clicked, outputEdit, &QPlainTextEdit::clear);
```

**KHÔNG làm**:
- Không auto-show khi có output mới — user tự bấm Show nếu muốn xem terminal.
- Không tự ẩn output sau N giây — user tự hide khi muốn.
- Không dùng toggle bar riêng 28px hoặc line counter; header 32px của outputPanel là vùng toggle duy nhất.
- Không đổi `pageSplitter` hay ẩn splitter handle thủ công; khi Show, output panel vẫn resize bằng splitter như cũ.

#### 4A.7 Main Content Widget — ví dụ Files Tab (Shell & Automation)

```
┌──────────────────────────────────────────────────────────┐
│  QSplitter(Horizontal) — explorerSplitter                 │
│                                                            │
│  ┌─────────────────────┐  ┌───────────────────────────┐  │
│  │ QTreeView           │  │ DETAIL / INFO PANEL        │  │
│  │ (file system tree)  │  │ (QLabel path, QLabel info) │  │
│  │                     │  │                            │  │
│  │ stretch = 3         │  │ stretch = 2                │  │
│  │                     │  │                            │  │
│  └─────────────────────┘  └───────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

Với các tab không dùng file tree (Cron Jobs, System Time, Packages):
```
┌──────────────────────────────────────────────────────────┐
│  #card (nền #FFFFFF border #E0DFDE radius 6px)            │
│  padding: 16px                                            │
│                                                            │
│  [QLabel section heading #sectionHeading]                 │
│  font-size 16px bold                                       │
│                                                            │
│  [content: QTableView / QLabel thông tin]                 │
│                                                            │
└──────────────────────────────────────────────────────────┘
```

Card phải dùng `objectName("card")` và nằm trong QScrollArea nếu content có thể dài.

#### 4A.8 SimpleFormDialog — chi tiết

```
┌─────────────────────────────────────────┐
│  QDialog (modal, minWidth=400)           │
│                                          │
│  [QLabel title] font-size:16px bold      │
│  ─────────────────────────────────────   │  ← QFrame separator
│                                          │
│  [QLabel "Source path"]                  │
│  [QLineEdit placeholder="/path/..."]     │
│                                          │
│  [QLabel "Destination path"]             │
│  [QLineEdit placeholder="/path/..."]     │
│                                          │
│  (nếu isDangerous=true):                 │
│  ⚠ Thao tác này không thể hoàn tác.     │  ← QLabel màu #C7162D
│                                          │
│  ──────────────────────────────────────  │
│  [Cancel]              [OK / Delete ×]   │  ← Cancel = #secondaryBtn
│                                    ↑         OK = #primaryBtn
│                          nếu dangerous = #dangerBtn
└─────────────────────────────────────────┘
```

Layout: QVBoxLayout, contentMargins 20px all sides, spacing 12px.

#### 4A.9 PATCH — Layout chi tiết từng tab (toàn bộ 3 page)

> Agent phải implement đúng layout từng tab theo spec này. Lỗi phổ biến nhất là để vùng giữa action bar và output panel **trống hoàn toàn** — mọi tab đều phải có main content widget lấp đầy vùng đó.

**Quy tắc bất biến cho MỌI tab:**
```cpp
// topArea = action bar (stretch=0) + main content (stretch=1)
QWidget* topArea = new QWidget(this);
QVBoxLayout* topLay = new QVBoxLayout(topArea);
topLay->setContentsMargins(0, 0, 0, 0);
topLay->setSpacing(8);
topLay->addWidget(actionBar,   0);  // KHÔNG co giãn
topLay->addWidget(mainContent, 1);  // PHẢI có stretch=1, lấp đầy phần còn lại

pageSplitter->addWidget(topArea);    // stretch=5
pageSplitter->addWidget(outputPanel); // stretch=1
```

**Empty state** dùng chung khi tab chưa có dữ liệu (thay vì để trắng):
```cpp
QWidget* emptyState = new QWidget();
QVBoxLayout* el = new QVBoxLayout(emptyState);
el->setAlignment(Qt::AlignCenter);
QLabel* icon = new QLabel("○");
icon->setAlignment(Qt::AlignCenter);
icon->setStyleSheet("font-size:28px; color:#C0BFBC;");
QLabel* hint = new QLabel("Nhấn nút để tải dữ liệu");
hint->setAlignment(Qt::AlignCenter);
hint->setStyleSheet("color:#6E6E6E; font-size:13px;");
el->addWidget(icon); el->addSpacing(6); el->addWidget(hint);
```

**Clear policy — áp dụng toàn bộ app:**

| Tình huống | Hành động |
|---|---|
| User nhấn nút load **khác loại** trong cùng tab (vd: Interfaces → Routes) | `netView->clear()` trước khi chạy lệnh, sau đó `setPlainText()` kết quả mới |
| User **switch tab** (Cron Jobs → System Time) | KHÔNG clear — mỗi tab giữ state riêng |
| User nhấn **cùng nút** lần 2 (vd: ps lần 2) | `clear()` rồi fill lại — luôn hiện kết quả mới nhất |
| Output **panel dưới** (tối) | Không tự clear — user dùng nút [× Clear] thủ công |

```cpp
// Pattern chuẩn cho mọi nút "load data mới":
connect(btn, &QPushButton::clicked, this, [this]() {
    targetView->clear();      // ← clear TRƯỚC khi chạy lệnh
    commandRunner->run(...);
});
```

**Auto-load policy — các tab có list/read:**

```cpp
// Gọi khi user mở page hoặc chuyển tab. Dùng lại đúng handler của nút refresh.
void ShellAutomationPage::onTabEntered(int tab) {
    if (tab == Files)      updateFileTreeRoot(/*runList=*/true);
    if (tab == CronJobs)   loadCronList();     // --cmd cron-list
    if (tab == SystemTime) loadTimeShow();     // --cmd time-show
    if (tab == LabLog)     loadLabLog();       // --cmd log
    // Packages: KHÔNG auto-load vì các action pkg cần sudo / có side effect.
}

void ProcessNetworkPage::onTabEntered(int tab) {
    if (tab == Processes) loadProcessList();   // --cmd ps
    if (tab == Network)   loadInterfaces();    // --cmd interfaces
}
```

Mỗi hàm auto-load phải có guard in-flight riêng (`cronListInFlight`, `timeShowInFlight`, `labLogInFlight`, `processListInFlight`, `networkInFlight`, ...). Nếu đang chạy thì return, không bắn thêm process trùng.

Riêng tab Files: `pathEdit.returnPressed` và `editingFinished` phải normalize path, nếu là thư mục hợp lệ thì đổi root của `QFileSystemModel` ngay. Khi user vào tab Files hoặc bấm List mới chạy `--cmd list <path>` để refresh output/list; không reload khi user chỉ đang gõ từng ký tự.

**KHÔNG làm**:
- Không auto-load Tab 1D Packages (tất cả pkg action cần sudo, có side effect)
- Không preload tất cả tab cùng lúc trong constructor; chỉ load tab đang mở / vừa được chọn

---

Tham khảo UX: **Nautilus (GNOME Files) browser mode** — address bar đồng bộ với tree, List refresh bằng CLI `--cmd list <dir>`, detail pane bên phải.

```
ADDRESS BAR (QHBoxLayout riêng, TRÊN main content, spacing=4):
  [←]  [→]  [↑]         ← QToolButton objectName="navBtn", h=30
  [QLineEdit pathEdit  stretch=1]   ← hiện đường dẫn hiện tại, Enter để navigate
  [↺ Refresh]           ← QToolButton objectName="navBtn"

  Back  [←]: pop historyBack → push currentPath vào historyForward → navigateTo()
  Fwd   [→]: pop historyForward → push currentPath vào historyBack  → navigateTo()
  Up    [↑]: QFileInfo(currentPath).dir().absolutePath()            → navigateTo()
  Refresh[↺]: fileTree->setRootIndex(fsModel->index(currentPath))  (không push history)

  Disable [←] nếu historyBack rỗng
  Disable [→] nếu historyForward rỗng
  Disable [↑] nếu currentPath == "/"

MAIN CONTENT (QSplitter Horizontal, stretch=1):
  ┌───────────────────────────────┬───────────────────────────────────────────┐
  │ QTreeView fileTree (stretch=2)│ DETAIL PANEL (stretch=3)                 │
  │                               │ QStackedWidget detailStack:              │
  │ Model: QFileSystemModel       │                                           │
  │   setRootPath(currentPath)    │  page 0 — EMPTY STATE                   │
  │   setFilter(                  │    QLabel "Chọn file để xem thông tin"   │
  │     QDir::AllEntries |        │    căn giữa, color:#6E6E6E               │
  │     QDir::NoDot)              │                                           │
  │   hiColumns: Name|Size|Date   │  page 1 — FILE / DIR INFO               │
  │   ẩn Type column              │    QLabel icon+tên  bold 14px            │
  │                               │      📄 nếu file / 📁 nếu dir           │
  │ NAVIGATION:                   │    QLabel "Đường dẫn: {path}"           │
  │   single-click  → chọn item   │    QLabel "Kích thước: {size}"          │
  │   double-click DIR → vào thư  │    QLabel "Sửa đổi: {mtime}"           │
  │     mục (navigateTo + history)│    QLabel "Quyền: {mode}"              │
  │   double-click FILE → không   │    ── separator ──                        │
  │     làm gì thêm (chỉ chọn)   │    [Stat]  [Read file*]  addStretch()    │
  │                               │    * ẩn nếu item là thư mục             │
  │ LIVE RELOAD (inotify):        │    objectName="actionBarBtn"             │
  │   QFileSystemModel tự watch   │                                           │
  │   thư mục hiện tại qua inotify│  Switch page:                            │
  │   → tree tự cập nhật khi có  │    chọn file/dir → page 1               │
  │   thay đổi trên disk mà KHÔNG │    bỏ chọn / navigate mới → page 0     │
  │   cần user nhấn Refresh       │                                           │
  └───────────────────────────────┴───────────────────────────────────────────┘

ACTION BAR (QHBoxLayout, BÊN DƯỚI address bar, spacing=8):
  [List]  [More▾]  addStretch()
  List chạy --cmd list <currentPath>; tree vẫn luôn hiện sẵn và tự sync theo pathEdit

void navigateTo(const QString& path) {
    historyBack.push(currentPath);   // QStack<QString>
    historyForward.clear();
    currentPath = path;
    pathEdit->setText(path);
    fileTree->setRootIndex(fsModel->index(path));
    detailStack->setCurrentIndex(0);  // reset detail về empty
    updateNavButtons();               // enable/disable ←→↑
}

// Xử lý user nhập tay vào pathEdit: Enter hoặc rời ô đều cập nhật tree root.
auto applyPathEdit = [this]() {
    QString p = pathEdit->text().trimmed();
    p = normalizePathAgainstRepoRoot(p);  // path tương đối tính từ repo root/current working dir
    if (QFileInfo(p).isDir()) {
        navigateTo(p);
    } else {
        // path không hợp lệ → flash đỏ 600ms
        pathEdit->setStyleSheet("border:1px solid #C7162D;");
        QTimer::singleShot(600, pathEdit, [this]{ pathEdit->setStyleSheet(""); });
        pathEdit->setText(currentPath);  // khôi phục path cũ
    }
};
connect(pathEdit, &QLineEdit::returnPressed, this, applyPathEdit);
connect(pathEdit, &QLineEdit::editingFinished, this, applyPathEdit);

connect(listBtn, &QPushButton::clicked, this, [this]() {
    runLab1("list", {currentPath}, "Lab1 List");  // đúng CODEBASE_SURVEY.md: --cmd list <dir>
});
```

**KHÔNG làm**:
- Không dùng `QDirModel` (deprecated) — chỉ dùng `QFileSystemModel`
- Không reload toàn bộ model mỗi lần user gõ ký tự — chỉ navigate khi Enter hoặc rời ô input
- Không hiển thị hidden files mặc định — toggle qua More menu
- Không double-click mở file bằng `xdg-open` hay bất kỳ lệnh nào
- Không đặt `setRootPath("/")` ngay từ đầu (chậm) — bắt đầu bằng repo root/current path đã normalize

More menu:
```
Show hidden files  ← QAction checkable; toggle NoDotAndDotDot / AllEntries
──────────────
Create file...
Create directory...
──────────────
Copy...
Move / Rename...
──────────────
Find...
Backup...
──────────────
Delete...          ← isDangerous=true
```

#### Tab 1B — Cron Jobs

```
ACTION BAR (QHBoxLayout, spacing=8):
  [Cron List]  [More▾]  addStretch()
  objectName="actionBarBtn" / "moreBtn"

MAIN CONTENT (stretch=1):
  QStackedWidget cronStack:
    page 0 — loading/empty state: "Đang tải danh sách cron..."
    page 1 — QTableView cronTable:
               columns: Name (stretch) | Expression (200px fixed) | Command (stretch)
               selectionMode = SingleRow
               Khi có row được chọn → enable nút Cron Remove trong More menu
               Khi không chọn gì   → disable nút Cron Remove

Khi chuyển vào tab Cron Jobs → tự chạy --cmd cron-list, không bắt user bấm Cron List lần đầu
Sau khi chạy cron-list thành công → parse output → populate cronTable → switch page 1
[Cron List] là nút refresh thủ công, dùng cùng loadCronList() và guard cronListInFlight
```

More menu:
```
Cron Add...
──────────────
Cron Remove...   ← disabled nếu chưa chọn row trong table
```

#### Tab 1C — System Time

```
ACTION BAR (QHBoxLayout, spacing=8):
  [Time Show]  [More▾]  addStretch()

MAIN CONTENT (stretch=1):
  #card (nền #FFFFFF, border #E0DFDE, radius 6px, padding 16px):
    QVBoxLayout, spacing=12:
      QLabel "Thông tin thời gian hệ thống" objectName="sectionHeading"
      ──────── QFrame separator ────────
      QLabel timeLabel  "—"  (cập nhật khi vào tab hoặc nhấn Time Show)
                             font-size:15px, color:#2C2C2C
      QLabel tzLabel    "Timezone: —"  color:#6E6E6E
      QLabel ntpLabel   "NTP: —"       color:#6E6E6E
      addStretch()   ← đẩy nội dung lên trên, không để trống ở giữa

Khi chuyển vào tab System Time → tự chạy --cmd time-show, không bắt user bấm Time Show lần đầu
Sau khi chạy time-show thành công → parse output → cập nhật 3 QLabel
[Time Show] là nút refresh thủ công, dùng guard timeShowInFlight
```

More menu:
```
Timezone Set...   ← badge "Cần sudo" trong dialog
Time Set...       ← isDangerous=true + badge "Cần sudo"
NTP Enable        ← badge "Cần sudo" (không cần input)
```

#### Tab 1D — Packages

```
ACTION BAR (QHBoxLayout, spacing=8):
  [More▾]  addStretch()
  (không có nút ngoài vì tất cả pkg action cần sudo)

MAIN CONTENT (stretch=1):
  #card (padding 16px):
    QLabel "Quản lý gói phần mềm" objectName="sectionHeading"
    ──── separator ────
    QLabel "Kết quả lệnh package sẽ hiện trong Output bên dưới."
           color:#6E6E6E, wordWrap=true
    QLabel "⚠ Tất cả lệnh cần sudo." color:#C7162D, font-size:12px
    addStretch()
```

More menu:
```
Update packages      ← badge "Cần sudo", không cần dialog
Install packages...  ← dialog: field "Tên gói (cách nhau dấu cách)"
Install from file... ← dialog: field "Đường dẫn file packages.txt"
──────────────
Remove packages...   ← isDangerous=true + badge "Cần sudo"
```

#### Tab 1E — Lab Log

```
ACTION BAR (QHBoxLayout, spacing=8):
  [Load Log]  addStretch()

MAIN CONTENT (stretch=1):
  QPlainTextEdit logView:
    readOnly=true, lineWrapMode=NoWrap
    font: "Ubuntu Mono" 12px
    nền #FAFAFA (sáng, khác với outputPanel tối)
    placeholder: "Đang tải 50 dòng log cuối..."

Khi chuyển vào tab Lab Log → tự chạy --cmd log
Sau khi chạy --cmd log → logView->setPlainText(output) để thay nội dung cũ, không append chồng
[Load Log] là nút refresh thủ công, dùng guard labLogInFlight
```

---

### PAGE 2 — Process & Network (ProcessNetworkPage)

#### Tab 2A — Processes

```
ACTION BAR (QHBoxLayout, spacing=8):
  [ps]  [More▾]  addStretch()
  ps objectName="actionBarBtn"

MAIN CONTENT (stretch=1):
  QStackedWidget procStack:
    page 0 — loading/empty state: "Đang tải danh sách tiến trình..."
    page 1 — QSplitter Horizontal:
      ┌──────────────────────────┬──────────────────────────────────┐
      │ QTableView procTable     │ PROC DETAIL (stretch=2)          │
      │ (stretch=3)              │ QStackedWidget:                  │
      │ cols: PID(80) | Name     │   page 0: "Chọn tiến trình"     │
      │       (stretch) | Status │   page 1:                        │
      │       (100)              │     QLabel "PID: {pid}" bold     │
      │ selectionMode=SingleRow  │     QLabel "Name: {name}"        │
      │                          │     QLabel "State: {state}"      │
      │                          │     QLabel "PPid: {ppid}"        │
      │                          │     QLabel "VmRSS: {vmrss}"      │
      │                          │     ──────────────────           │
      │                          │     [proc-info]  addStretch()    │
      └──────────────────────────┴──────────────────────────────────┘

Khi mở Process & Network page hoặc chuyển vào tab Processes → tự chạy --cmd ps
[ps] là nút refresh thủ công, dùng guard processListInFlight
connect(procTable selection) → gọi proc-info <pid> → parse → cập nhật detail panel
```

More menu:
```
Kill process...   ← isDangerous=true; dialog: field PID (pre-fill từ row đang chọn),
                     field Signal (default 15/SIGTERM)
                     disabled nếu chưa chọn row
```

#### Tab 2B — Files (Process & Network)

```
ACTION BAR (QHBoxLayout, spacing=8):
  [QLineEdit pathEdit stretch=1]  [Stat]  [Read file]  [More▾]
  (3 widget ngoài vì Stat + Read là read-only, an toàn)

MAIN CONTENT (stretch=1):
  #card (padding 16px):
    QLabel "Thao tác file" objectName="sectionHeading"
    ── separator ──
    QLabel pathInfoLabel  "—"  (hiện kết quả stat)
           wordWrap=true, font:"Ubuntu Mono" 12px, color:#2C2C2C
    addStretch()

Nhấn Stat  → chạy --cmd stat {pathEdit}  → hiện kết quả trong pathInfoLabel
Nhấn Read  → chạy --cmd read-file {path} → hiện kết quả trong outputPanel bên dưới
```

More menu:
```
Write file...    ← dialog: Path, Content (QPlainTextEdit trong dialog)
Copy file...     ← dialog: Source, Destination
Chmod...         ← dialog: Path, Mode (placeholder "644")
──────────────
Delete file...   ← isDangerous=true; dialog: Path
```

#### Tab 2C — Network

Tham khảo UX: **GNOME System Monitor Resources tab** — mỗi lần switch nguồn thông tin thì clear output cũ, không append lẫn lộn.

```
ACTION BAR (QHBoxLayout, spacing=8):
  [Interfaces]  [Routes]  [More▾]  addStretch()
  objectName="actionBarBtn"

  QUAN TRỌNG — Clear behavior:
  Nhấn [Interfaces] → clear toàn bộ netView → chạy --cmd interfaces → hiện kết quả mới
  Nhấn [Routes]     → clear toàn bộ netView → chạy --cmd routes     → hiện kết quả mới
  → KHÔNG append vào output cũ; mỗi lần nhấn là một lần xem sạch
  Khi chuyển vào tab Network → tự chạy --cmd interfaces; [Interfaces] chỉ là refresh thủ công

MAIN CONTENT (QSplitter Vertical, stretch=1):
  ┌──────────────────────────────────────────────────────────────────────────┐
  │ HEADER ROW (h=32, nền #F2F1F0, padding 8px 12px):                        │
  │   QLabel sourceLabel "—"  ← cập nhật: "Interfaces" / "Routes" / ...     │
  │   font-weight:600, color:#2C2C2C                        addStretch()     │
  │   QLabel timeLabel "—"  ← timestamp lần chạy gần nhất, color:#6E6E6E    │
  ├──────────────────────────────────────────────────────────────────────────┤
  │ QPlainTextEdit netView (stretch=1):                                       │
  │   readOnly=true, lineWrapMode=NoWrap                                      │
  │   font:"Ubuntu Mono" 11px, nền #FAFAFA                                   │
  │   placeholder: "Nhấn Interfaces hoặc Routes để xem thông tin mạng."      │
  └──────────────────────────────────────────────────────────────────────────┘

  connect(interfacesBtn, &QPushButton::clicked, this, [this]() {
      netView->clear();                            // ← BẮT BUỘC clear trước
      sourceLabel->setText("Interfaces");
      timeLabel->setText("...");
      commandRunner->run(ubuntuToolPath,
          {"--cmd", "interfaces"},
          CommandType::OneShot, TrustLevel::Reliable);
  });

  connect(routesBtn, &QPushButton::clicked, this, [this]() {
      netView->clear();                            // ← BẮT BUỘC clear trước
      sourceLabel->setText("Routes");
      timeLabel->setText("...");
      commandRunner->run(ubuntuToolPath,
          {"--cmd", "routes"},
          CommandType::OneShot, TrustLevel::Reliable);
  });

  // Khi CommandRunner trả về output:
  connect(commandRunner, &CommandRunner::outputReady, netView,
      [this](const QString& out) {
          netView->setPlainText(out);
          timeLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
      });
```

More menu:
```
Resolve hostname...  ← dialog: field "Hostname"; clear netView trước khi chạy, rồi setPlainText kết quả mới
TCP connect test...  ← dialog: "Host", "Port"; clear netView trước khi chạy, rồi setPlainText kết quả mới
```

#### Tab 2D — Sockets

```
ACTION BAR: KHÔNG có action bar truyền thống ở đây
  → Toàn bộ main content là 2 card ngang nhau (TCP + UDP)

MAIN CONTENT (stretch=1):
  QHBoxLayout, spacing=12:
  ┌─────────────────────────────────┐  ┌─────────────────────────────────┐
  │ #card TCP Server (stretch=1)    │  │ #card UDP Receiver (stretch=1)  │
  │ padding 16px                    │  │ padding 16px                    │
  │                                 │  │                                 │
  │ QLabel "TCP Server"             │  │ QLabel "UDP Receiver"           │
  │ objectName="sectionHeading"     │  │ objectName="sectionHeading"     │
  │ ── separator ──                 │  │ ── separator ──                 │
  │ QLabel statusLabel "● Dừng"    │  │ QLabel statusLabel "● Dừng"    │
  │   color:#6E6E6E                 │  │   color:#6E6E6E                 │
  │   khi running: "● Đang chạy"   │  │   khi running: "● Đang chạy"   │
  │   color:#1E7D32                 │  │   color:#1E7D32                 │
  │                                 │  │                                 │
  │ QLabel "Port:" + QSpinBox       │  │ QLabel "Port:" + QSpinBox       │
  │   range 1024-65535, default 8080│  │   range 1024-65535, default 9090│
  │   disabled khi đang chạy        │  │   disabled khi đang chạy        │
  │                                 │  │                                 │
  │ [Start TCP Server] primaryBtn   │  │ [Start UDP Receiver] primaryBtn │
  │   ← khi chạy: [Stop] dangerBtn │  │   ← khi chạy: [Stop] dangerBtn │
  │                                 │  │                                 │
  │ addStretch()                    │  │ addStretch()                    │
  │ ── separator ──                 │  │ ── separator ──                 │
  │ [TCP Client Test...]            │  │ [UDP Send...]                   │
  │   actionBarBtn (mở dialog)      │  │   actionBarBtn (mở dialog)      │
  └─────────────────────────────────┘  └─────────────────────────────────┘

Start → ProcessRegistry.start() → track PID → cập nhật status + toggle button
Stop  → ProcessRegistry.stop(pid) → cập nhật status + toggle button
```

---

### PAGE 3 — Kernel Modules (KernelModulesPage)

> Toàn bộ page disabled (overlay "Cần chạy bằng sudo") nếu `geteuid() != 0`.

#### Tab 3A — Netfilter Modules

```
ACTION BAR: không có (action trực tiếp trên từng card)

MAIN CONTENT (stretch=1, QScrollArea):
  QVBoxLayout, spacing=12:

  Mỗi module = 1 #card dạng hàng ngang:
  ┌───────────────────────────────────────────────────────────────────┐
  │ #card (padding 12px 16px)                                         │
  │ QHBoxLayout:                                                      │
  │   QLabel "packet_logger"  bold  (stretch=1)                      │
  │   QLabel statusDot  "●"  color:#6E6E6E (Unloaded)                │
  │                          color:#1E7D32 (Loaded)                   │
  │   [Build]  [Load]  [Unload]                                       │
  │   objectName="actionBarBtn"                                       │
  │   Build: disabled sau khi đã build thành công (1 lần/session)    │
  │   Load:  disabled nếu đang Loaded HOẶC chưa Build                │
  │   Unload:disabled nếu đang Unloaded                               │
  └───────────────────────────────────────────────────────────────────┘

  Thứ tự các card (từ trên xuống):
  1. packet_logger      (cwd: 1-2-netfilter/packet_logger)
  2. dst_filter         (cwd: 1-2-netfilter/dst_filter)
     → khi Loaded: hiện thêm inline panel filterctl bên dưới card
  3. tcp_listen         (cwd: 3-4-tcp-sock/tcp_listen)
  4. tcp_accept         (cwd: 3-4-tcp-sock/tcp_accept)
     → Load disabled thêm nếu tcp_listen đang Loaded
  5. udp_sender         (cwd: 5-udp-sock/udp_sender)
     → note trong card: "insmod gửi 1 UDP packet ngay lập tức"

filterctl inline panel (chỉ hiện khi dst_filter Loaded):
  ┌─────────────────────────────────────────────┐
  │ [QLineEdit "IP filter (IPv4)"]  [Set]  [Clear]  [Status]│
  │ objectName="actionBarBtn"                              │
  └─────────────────────────────────────────────┘
```

#### Tab 3B — Steganography Module

```
MAIN CONTENT (stretch=1):
  QVBoxLayout, spacing=12:

  #card tcp_udp_steg (cùng kiểu module card ở 3A):
    [Build]  [Load]  [Unload]  + statusDot

  #card stegctl (chỉ enable khi tcp_udp_steg Loaded):
    QLabel "Steganography Control" sectionHeading
    ── separator ──
    QLabel "Message (tối đa 256 byte):"
    QLineEdit msgEdit  maxLength=256
    QHBoxLayout: [Set Message]  [Clear]  [Status]  addStretch()
    objectName="actionBarBtn"
```

#### Tab 3C — Firewall

```
MAIN CONTENT (stretch=1, QScrollArea):
  QVBoxLayout, spacing=12:

  #card — Firewall Modules:
    QLabel "Kernel Modules" sectionHeading
    ── separator ──
    [card hàng ngang local_fw]   (cùng kiểu tab 3A)
    [card hàng ngang forward_fw] (Unload theo thứ tự: forward_fw → local_fw)

  #card — IP Forwarding:
    QLabel "IP Forwarding" sectionHeading
    ── separator ──
    QLabel fwdStatus  "Trạng thái: —"
    QHBoxLayout: [Setup Forwarding]  [Disable Forwarding]  addStretch()
    objectName="actionBarBtn"

  #card — Firewall Control (enable khi ít nhất 1 fw module Loaded):
    QLabel "Firewall Control" sectionHeading
    ── separator ──
    QHBoxLayout: [Status]  [Stats]  [More▾]  addStretch()

    QFrame separator
    QLabel "Add Rule" font-weight:600
    FORM (QFormLayout):
      Action:   QComboBox [log | accept | drop]
      Target:   QComboBox [local | forward | both]
      Protocol: QComboBox [any | tcp | udp | icmp]
                → onChange: nếu icmp, ép src-port/dst-port = "any", disable 2 field đó
      Src IP:   QLineEdit placeholder="any hoặc IPv4"
      Dst IP:   QLineEdit placeholder="any hoặc IPv4"
      Src Port: QLineEdit placeholder="any hoặc 1-65535"
      Dst Port: QLineEdit placeholder="any hoặc 1-65535"

    [Add Rule]  ← validate client-side trước khi gửi
                  nếu action=drop → confirm dialog isDangerous=true trước khi gửi
    objectName="primaryBtn"

  More menu (Firewall Control):
    Status local / Status forward / Status both
    Stats local / Stats forward / Stats both
    ──────────────
    Clear rules...   ← isDangerous=true; dialog: chọn local/forward/both

  #card — dmesg Logs:
    QLabel "Kernel Log" sectionHeading
    ── separator ──
    QPlainTextEdit dmesgView readOnly, font:"Ubuntu Mono" 12px, nền #FAFAFA
    placeholder: "Nhấn Refresh để lấy log dmesg lọc theo local_fw / forward_fw"
    QHBoxLayout: [Refresh dmesg]  addStretch()
```

#### Tab 3D — Edge Firewall

```
MAIN CONTENT (stretch=1):
  QVBoxLayout, spacing=12:

  #card — Scripts:
    QLabel "Edge Firewall Scripts" sectionHeading
    ── separator ──
    QLabel "Cấu hình IP forwarding cho edge firewall VM."
           color:#6E6E6E, wordWrap=true
    QLabel "⚠ Có thể ảnh hưởng routing máy test."
           color:#C7162D, font-size:12px
    QHBoxLayout: [Setup Forwarding]  [Disable Forwarding]  addStretch()
    objectName="actionBarBtn"

  #card — Test:
    QLabel "Smoke Test" sectionHeading
    ── separator ──
    QLabel "Chạy test_edge_firewall.sh — cleanup tự động sau khi test."
           color:#6E6E6E
    QHBoxLayout: [Run Test]  addStretch()
    objectName="dangerBtn"   (vì có side effect rmmod + fwctl clear)
```

---

### PAGE 4 — Demo Scenarios (DemoScenariosPage)

```
MAIN CONTENT (không có tab bar, toàn page là 1 layout):
  ACTION BAR: không có
  
  QScrollArea → QVBoxLayout, spacing=12:
    Mỗi scenario = 1 #card:
    ┌───────────────────────────────────────────────────────┐
    │ #card (padding 16px)                                  │
    │ QLabel "{Tên scenario}" sectionHeading                │
    │ QLabel "{Mô tả ngắn}" color:#6E6E6E, wordWrap=true   │
    │ ── separator ──                                       │
    │ QLabel "Các bước:" font-weight:600                    │
    │ QLabel "1. {bước 1}\n2. {bước 2}..." color:#2C2C2C  │
    │ QHBoxLayout: [Run Scenario]  addStretch()             │
    │               objectName="primaryBtn"                 │
    └───────────────────────────────────────────────────────┘

  pageSplitter: topArea (scenarios list, stretch=5) | outputPanel (stretch=1)
```

---

### PAGE 5 — Activity Log (ActivityLogPage)

```
KHÔNG có tab bar (toàn page là 1 view)

ACTION BAR (QHBoxLayout, spacing=8):
  [Clear Log]  addStretch()
  objectName="secondaryBtn"

MAIN CONTENT (stretch=1):
  QTableView activityTable:
    columns: Time (160px fixed) | Level (80px fixed) | Action (200px)
             | Detail (stretch) | Exit (60px fixed)
    selectionMode = SingleRow
    header background #F2F1F0, font-weight:600
    row color: Exit!=0 → text #C7162D (lỗi); Unreliable → text #6E6E6E

pageSplitter: topArea (stretch=5) | outputPanel (stretch=1)
  → click row trong table → hiện full output của lệnh đó trong outputPanel
```

---

## 5. Layout kéo giãn — QSplitter ở mọi cấp

### Cấp MainWindow
```cpp
QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
mainSplitter->addWidget(sidebar);
mainSplitter->addWidget(contentArea);
mainSplitter->setStretchFactor(0, 1);   // sidebar
mainSplitter->setStretchFactor(1, 4);   // content
mainSplitter->setChildrenCollapsible(false);
setCentralWidget(mainSplitter);
```
Sidebar: `setMinimumWidth(180)`, `setMaximumWidth(320)`.

### Cấp mỗi Page
```cpp
// Layout tổng của page: VBoxLayout
// [tabBar]        ← cố định trên (KHÔNG trong splitter)
// [pageSplitter]  ← chiếm phần còn lại

QSplitter* pageSplitter = new QSplitter(Qt::Vertical, this);
pageSplitter->addWidget(topArea);      // action bar + main widget
pageSplitter->addWidget(outputPanel);  // header visible, body hidden mặc định
pageSplitter->setStretchFactor(0, 5);
pageSplitter->setStretchFactor(1, 1);
pageSplitter->setChildrenCollapsible(false);
outputPanel->setMinimumHeight(32);
outputPanel->setMaximumHeight(32);     // bấm Show mới mở minH=80 và cho kéo bằng splitter
```

### File Explorer (nếu dùng 2 cột)
```cpp
QSplitter* explorerSplitter = new QSplitter(Qt::Horizontal, this);
explorerSplitter->addWidget(folderTreeView);
explorerSplitter->addWidget(detailsArea);
explorerSplitter->setStretchFactor(0, 3);
explorerSplitter->setStretchFactor(1, 2);
explorerSplitter->setChildrenCollapsible(false);
```

### Style splitter handle (thêm vào `style.qss`)
```css
QSplitter::handle { background-color: #E0DFDE; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }
QSplitter::handle:hover { background-color: #E95420; }
QSplitter::handle:pressed { background-color: #C7401A; }
```

### Lưu vị trí splitter
```cpp
// Đóng app
QSettings settings("LabCuoiKy", "QtDashboard");
settings.setValue("mainSplitter/state", mainSplitter->saveState());
// Mở app trước show()
if (settings.contains("mainSplitter/state"))
    mainSplitter->restoreState(settings.value("mainSplitter/state").toByteArray());
```

---

## 6. Action UI — 1-2 nút ngoài, còn lại vào "More ▾"

### Bảng nhóm action theo page

| Page | Để ngoài (luôn thấy) | Gộp vào "More ▾" |
|---|---|---|
| Shell & Automation — Files | `List` | Create file, Create directory, Copy, Move/Rename, Find, Backup, Delete |
| Shell & Automation — Cron Jobs | `Cron List` | Cron Add, Cron Remove |
| Shell & Automation — System Time | `Time Show` | Timezone Set, Time Set, NTP Enable |
| Shell & Automation — Packages | — | Update, Install, Install từ file, Remove |
| Process & Network — Processes | `ps` | Kill |
| Process & Network — Files | `Stat`, `Read file` | Write file, Copy file, Chmod, Delete file |
| Process & Network — Network | `Interfaces`, `Resolve`, `Routes` | — |
| Process & Network — Sockets | `Start/Stop TCP server`, `Start/Stop UDP receiver` | TCP client test, UDP send |
| Kernel Modules — Build/Modules | Load/Unload trực tiếp (không qua More) | — |
| Kernel Modules — Firewall Rules | `Status`, `Stats` | Clear, Add rule |

### Code mẫu: QToolButton + QMenu
```cpp
QToolButton* moreBtn = new QToolButton(this);
moreBtn->setObjectName("moreBtn");
moreBtn->setText("More ▾");
moreBtn->setPopupMode(QToolButton::InstantPopup);

QMenu* moreMenu = new QMenu(moreBtn);
moreMenu->setObjectName("moreMenu");
moreMenu->addAction("Create file...");
moreMenu->addAction("Create directory...");
moreMenu->addSeparator();
moreMenu->addAction("Copy...");
moreMenu->addAction("Move / Rename...");
moreMenu->addSeparator();
moreMenu->addAction("Find...");
moreMenu->addAction("Backup...");
moreMenu->addSeparator();
moreMenu->addAction("Delete...");   // nguy hiểm — luôn cuối, sau separator riêng

moreBtn->setMenu(moreMenu);
```

### Style QToolButton + QMenu (thêm vào `style.qss`)
```css
QToolButton#moreBtn {
    min-height: 32px;
    padding: 0px 12px;
    background-color: #FFFFFF;
    color: #2C2C2C;
    border: 1px solid #D6D5D4;
    border-radius: 6px;
    font-size: 13px;
}
QToolButton#moreBtn:hover { background-color: #F2F1F0; }
QToolButton#moreBtn::menu-indicator {
    width: 10px;
    subcontrol-position: right center;
    subcontrol-origin: padding;
    right: 6px;
}

QMenu#moreMenu {
    background-color: #FFFFFF;
    border: 1px solid #E0DFDE;
    border-radius: 6px;
    padding: 4px;
}
QMenu#moreMenu::item {
    padding: 8px 16px;
    border-radius: 4px;
    font-size: 13px;
    color: #2C2C2C;
}
QMenu#moreMenu::item:selected { background-color: #FCE4D9; color: #E95420; }
QMenu#moreMenu::separator { height: 1px; background-color: #E0DFDE; margin: 4px 8px; }
```

### Action nút ngoài — style `#actionBarBtn`
```css
QPushButton#actionBarBtn {
    min-height: 32px;
    padding: 0px 14px;
    font-size: 13px;
    background-color: #FFFFFF;
    color: #2C2C2C;
    border: 1px solid #D6D5D4;
    border-radius: 6px;
}
QPushButton#actionBarBtn:hover { background-color: #F2F1F0; }
```

### Action cần input → `SimpleFormDialog`, không dùng `QInputDialog`
```cpp
class SimpleFormDialog : public QDialog {
public:
    SimpleFormDialog(const QString& title,
                     const QList<QPair<QString,QString>>& fields, // {label, placeholder}
                     bool isDangerous = false,
                     QWidget* parent = nullptr);
    QStringList values() const;
};
```

```cpp
connect(copyAction, &QAction::triggered, this, [this]() {
    auto* dlg = new SimpleFormDialog(
        "Copy",
        {{"Source", "/path/to/src"}, {"Destination", "/path/to/dst"}},
        false, this);
    if (dlg->exec() == QDialog::Accepted) {
        QStringList v = dlg->values();
        commandRunner->run(adminToolPath,
            {"--cmd", "copy", v[0], v[1]},
            CommandType::OneShot, TrustLevel::Reliable);
    }
    dlg->deleteLater();
});
```

Action nguy hiểm (`Delete`, `Cron Remove`, `Package Remove`, `fwctl drop/clear`) dùng `isDangerous = true` — dialog thêm dòng cảnh báo + nút `#dangerBtn`. Dialog này là bước confirm duy nhất, không cần thêm `QMessageBox` sau.

---

## 7. Mapping UI ↔ Lệnh thật (theo `CODEBASE_SURVEY.md`)

### 7.1 Shell & Automation — `01_shell_system_admin/scripts/admin_tool.sh`

| Action | Lệnh | Loại | TrustLevel |
|---|---|---|---|
| List | `--cmd list <dir>` | OneShot | Reliable |
| Create file | `--cmd create-file <path>` | OneShot | Reliable |
| Create directory | `--cmd create-dir <path>` | OneShot | Reliable |
| Copy | `--cmd copy <src> <dst>` | OneShot | Reliable |
| Move/Rename | `--cmd move <src> <dst>` | OneShot | Reliable |
| Delete (nguy hiểm) | `--cmd delete <path> --yes` | OneShot | Reliable |
| Find | `--cmd find <dir> <pattern>` | OneShot | Reliable |
| Backup | `--cmd backup <src-dir> <dst-dir>` | OneShot | Reliable |
| Cron List | `--cmd cron-list` | OneShot | Reliable |
| Cron Add | `--cmd cron-add <name> <expr> <command>` | OneShot | Reliable — `<command>` là 1 argv trong QStringList |
| Cron Remove (nguy hiểm) | `--cmd cron-remove <name> --yes` | OneShot | Reliable |
| Time Show | `--cmd time-show` | OneShot | Reliable |
| Timezone Set | `--cmd timezone-set <zone> --yes` | OneShot | **Unreliable** |
| Time Set (nguy hiểm) | `--cmd time-set "<YYYY-MM-DD HH:MM:SS>" --yes` | OneShot | **Unreliable** |
| NTP Enable | `--cmd ntp-enable --yes` | OneShot | **Unreliable** |
| Pkg Update | `--cmd pkg-update --yes` | OneShot | **Unreliable** |
| Pkg Install | `--cmd pkg-install <pkg...> --yes` | OneShot | **Unreliable** |
| Pkg Install File | `--cmd pkg-install-file <file> --yes` | OneShot | **Unreliable** |
| Pkg Remove (nguy hiểm) | `--cmd pkg-remove <pkg...> --yes` | OneShot | **Unreliable** |
| Log | `--cmd log` | OneShot | Reliable |

**TrustLevel = Unreliable** → UI hiển thị "Đã chạy xong, xem output" — không tự gắn nhãn ✓/✗.

### 7.2 Process & Network — `02_ubuntu_process_file_socket_network/ubuntu_sys_tool`

| Action | Lệnh | Loại | TrustLevel |
|---|---|---|---|
| ps | `--cmd ps` | OneShot | Reliable |
| proc-info | `--cmd proc-info <pid>` | OneShot | Reliable |
| Kill (nguy hiểm) | `--cmd kill <pid> [signal]` | OneShot | Reliable |
| stat | `--cmd stat <path>` | OneShot | Reliable |
| read-file | `--cmd read-file <path>` | OneShot | Reliable |
| write-file | `--cmd write-file <path> "<text>"` | OneShot | Reliable |
| copy-file | `--cmd copy-file <src> <dst>` | OneShot | Reliable |
| chmod | `--cmd chmod <path> <mode>` | OneShot | Reliable |
| delete-file (nguy hiểm) | `--cmd delete-file <path> --yes` | OneShot | Reliable |
| interfaces | `--cmd interfaces` | OneShot | Reliable |
| resolve | `--cmd resolve <host>` | OneShot | Reliable |
| tcp-connect | `--cmd tcp-connect <host> <port>` | OneShot | Reliable |
| routes | `--cmd routes` | OneShot | Reliable |
| Start TCP server | `--cmd tcp-server <port>` | **LongRunning** | Ready: `Dang listen TCP port` |
| Stop TCP server | SIGTERM qua ProcessRegistry | — | — |
| tcp-client | `--cmd tcp-client <host> <port> "<message>"` | OneShot | Reliable |
| Start UDP receiver | `--cmd udp-receiver <port>` | **LongRunning** | Ready: `Dang nhan UDP port` |
| Stop UDP receiver | SIGTERM qua ProcessRegistry | — | — |
| udp-send (chỉ IPv4) | `--cmd udp-send <ipv4> <port> "<message>"` | OneShot | Reliable |

### 7.3 Kernel Modules & Firewall — `03_kernel_module_networking/`

> Toàn bộ cần `sudo`. Message lỗi insmod/rmmod chưa xác minh trên VM.

| Module | Build | Load | Unload | TrustLevel |
|---|---|---|---|---|
| `packet_logger` | `make` (cwd `1-2-netfilter/packet_logger`) | `sudo insmod packet_logger.ko` | `sudo rmmod packet_logger` | Unreliable |
| `dst_filter` | `make`/`make module`,`make user` (cwd `1-2-netfilter/dst_filter`) | `sudo insmod dst_filter.ko` | `sudo rmmod dst_filter` | Unreliable |
| `tcp_listen` | `make` (cwd `3-4-tcp-sock/tcp_listen`) | `sudo insmod tcp_listen.ko` | `sudo rmmod tcp_listen` | Unreliable |
| `tcp_accept` | `make` (cwd `3-4-tcp-sock/tcp_accept`) | `sudo insmod tcp_accept.ko` | `sudo rmmod tcp_accept` | Unreliable — disable nút Load nếu `tcp_listen` đang loaded |
| `udp_sender` | `make` (cwd `5-udp-sock/udp_sender`) | `sudo insmod udp_sender.ko` | `sudo rmmod udp_sender` | Unreliable |
| `tcp_udp_steg` | `make`/`make module`,`make user` (cwd `6-tcp-udp-steg`) | `sudo insmod tcp_udp_steg.ko` | `sudo rmmod tcp_udp_steg` | Unreliable |
| `local_fw` | `make`/`make module`,`make user`,`make scripts` (cwd `edge_firewall`) | `sudo insmod local_fw.ko` | `sudo rmmod local_fw` | Unreliable |
| `forward_fw` | (cùng cwd `edge_firewall`) | `sudo insmod forward_fw.ko` | `sudo rmmod forward_fw` | Unreliable — unload `forward_fw` trước `local_fw` |

**`filterctl`** (cwd `1-2-netfilter/dst_filter`): `set <ipv4>`, `clear`, `status` — Reliable.

**`stegctl`** (cwd `6-tcp-udp-steg`): `set <message...>` (max 256 byte, validate UI trước), `clear`, `status` — Reliable.

**`fwctl`** (cwd `edge_firewall`):

```
fwctl <log|accept|drop> <local|forward|both> <any|tcp|udp|icmp>
      <src-ip|any> <dst-ip|any> <src-port|any> <dst-port|any>
```

Validate client-side trước khi gửi:
- Protocol `icmp` → ép `src-port`/`dst-port` = `any`, disable 2 input.
- IP field chỉ nhận IPv4 literal hoặc `any`.
- Port field chỉ nhận `any` hoặc số 1–65535.
- `target = both` gửi 1 lệnh, fwctl tự tách.

**Logs**: chạy `dmesg` thuần, filter dòng bằng `QString::contains()`, không dùng pipe.

---

## 8. Quyền root / Limited mode

- **Lab 1**: hầu hết chạy user thường; chỉ `timezone-set`, `time-set`, `ntp-enable`, `pkg-*` cần sudo — hiện nút luôn bật kèm badge "Cần sudo".
- **Lab 2**: lỗi quyền qua `perror` (`Operation not permitted`, `Permission denied`) — tô đỏ kết quả.
- **Lab 3**: nếu app không chạy bằng sudo, **disable toàn bộ trang Kernel Modules** (không disable lẻ), hiện hướng dẫn:
  ```
  xhost +SI:localuser:root && sudo -E QT_QPA_PLATFORM=xcb ./qt_dashboard
  ```
- Header hiện badge `#badgeRoot` / `#badgeLimited`.

---

## 9. Thứ tự triển khai

1. **Khung sườn**: MainWindow + `mainSplitter` + header + sidebar + tab bar segmented + load `style.qss`. Verify đúng Yaru trước khi đụng backend.
2. **`CommandRunner` + `ProcessRegistry` + `ActivityLogger` + `SimpleFormDialog`**: test với `echo`/`sleep` giả lập; test TrustLevel hiển thị đúng; test SimpleFormDialog đúng theme.
3. **Shell & Automation Page**: từng tab theo bảng 7.1. Pattern `[action-để-ngoài] [More ▾]` + `pageSplitter` ngay từ đầu.
4. **Process & Network Page**: theo bảng 7.2; ready-signal pattern cho `tcp-server`/`udp-receiver`.
5. **Kernel Modules Page**: làm sau cùng theo thứ tự:
   a. Build module → b. Load/Unload từng module → c. `filterctl`/`stegctl` → d. `fwctl` form rule → e. Edge firewall scripts → f. `dmesg` filter.
6. **Demo Scenarios Page**: ghép action đã có.
7. **Activity Log Page**: hiển thị log từ bước 2.
8. **Test trên Ubuntu VM**: resize kiểm tra splitter, path có dấu cách, root/limited mode, không leak process, toàn bộ Lab 3.

---

## 10. Checklist hoàn thiện

- [ ] Theme đúng Yaru: cam `#E95420` duy nhất, font Ubuntu/Noto Sans, card radius 6px đồng nhất.
- [ ] Sidebar: QPushButton checkable, text-align left, border-left 3px cam khi checked, không dùng QListWidget.
- [ ] Header: QHBoxLayout, icon + title + stretch + badge, không có subtitle dòng 2, không có menu bar.
- [ ] Tab bar: segmented control QButtonGroup + QPushButton checkable, gom sang trái bằng addStretch() cuối, không dùng QTabWidget.
- [ ] Action bar: tối đa 2 nút ngoài + 1 More button, addStretch() cuối, không full-width.
- [ ] Mọi vùng cạnh nhau dùng QSplitter, kéo được, không collapse về 0, vị trí giữ qua QSettings.
- [ ] Output panel: mặc định hidden header 32px; khi Show minH=80, nền tối, monospace, resize bằng splitter.
- [ ] Files tree tự sync theo pathEdit khi Enter/rời ô và tự list khi vào tab.
- [ ] Các tab có dữ liệu list/read mặc định tự load khi vào tab, không bắt user bấm nút lần đầu.
- [ ] Network output clear trước khi hiển thị kết quả mới, không append chồng Interfaces/Routes cũ.
- [ ] Action cần input mở SimpleFormDialog đúng theme, không dùng QInputDialog.
- [ ] Action nguy hiểm: isDangerous=true + #dangerBtn trong dialog.
- [ ] TrustLevel Unreliable không tự gắn nhãn thành công/thất bại.
- [ ] Mapping lệnh đúng theo CODEBASE_SURVEY.md, không còn `?` hay lệnh tự đoán.
- [ ] Không tự `rmmod` module loaded từ trước khi mở dashboard.
- [ ] Không tính năng nào bị cắt so với 3 lab gốc.

---

## 11. Nguyên tắc xuyên suốt cho agent

- **Không tự đổi hành vi 3 lab gốc** — dashboard chỉ là lớp gọi và hiển thị.
- **Không tự thêm màu/font-size/radius/spacing mới** ngoài `style.qss` đã có.
- **Không tự bịa tên lệnh/tham số** ngoài `CODEBASE_SURVEY.md` — thiếu thì dừng và hỏi.
- **Không dùng QTabWidget** — chỉ dùng segmented control tự vẽ.
- **Không dùng QInputDialog** — chỉ dùng SimpleFormDialog.
- **Không full-width nút action** — luôn addStretch() cuối action bar.
- **Không xếp action button theo QVBoxLayout** — luôn QHBoxLayout ngang.
- Mỗi bước ở mục 9 làm xong phải build và chạy thử thật trước khi sang bước tiếp.
- Nếu phát hiện hành vi thật khác với survey lúc test VM, cập nhật lại bảng mapping.
