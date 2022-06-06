#define WINVER 0x0500 // Windows 2000
#ifdef _WIN32_WINNT
  #undef _WIN32_WINNT
#endif
#define _WIN32_WINNT WINVER

#define UNICODE

#include <windows.h>
#include <WinDef.h>
#include <stdio.h>

UINT c_color_cursor_scale = 1;
UINT c_bw_cursor_scale = 1;
UINT c_desired_r = 0;
UINT c_desired_g = 0;
UINT c_desired_b = 0;
double c_weight = 0.0;
UINT c_transparency_threshold = 1;
double c_screen_scale_factor = 1.0;
UINT c_show_small_cursor = 1;

const wchar_t c_win_class[]  = L"cursor_window";

HWND g_cursor_window = NULL;
HBITMAP g_cursor_bitmap = NULL;
UINT g_cursor_scale = 1;
HCURSOR g_last_cursor;
DWORD g_last_cursor_id = 0;
UINT g_pending_system_cursor_change = 0;
CRITICAL_SECTION g_lock;
BYTE* g_last_transparent_cursor_buf = NULL;

DWORD g_hotspot_x;
DWORD g_hotspot_y;
int g_width;
int g_height;

int g_max_cursor_x;
int g_max_cursor_y;

typedef struct cached_cursor_t
{
  DWORD id;
  HCURSOR original_cursor;
  HCURSOR cached_cursor;
} cached_cursor_t;

cached_cursor_t g_cached_system_cursors[] = {{.id=32512},{.id=32645},{.id=32513},{.id=32644},{.id=32643},{.id=32642},{.id=32649},{.id=32650},{.id=32515},{.id=32646},{.id=32514},{.id=32651},{.id=32641},{.id=32648},{.id=32640},{.id=32516}};

void error(char* msg)
{
  FILE* fp = fopen("debug.log", "a");
  unsigned long long now = time(NULL);
  fprintf(fp, "ERROR [%llu] [%d]: %s\n", now, GetLastError(), msg);
  fflush(fp);
  fclose(fp);
  exit(-1);
}

HCURSOR get_cached_system_cursor(HCURSOR cursor)
{
  const auto count = sizeof(g_cached_system_cursors)/sizeof(cached_cursor_t);
  for (UINT i = 0; i < count; ++i)
  {
    if (cursor == g_cached_system_cursors[i].original_cursor)
    {
      return g_cached_system_cursors[i].cached_cursor;
    }
  }
  return NULL;
}

void update_cursor_if_changed()
{
  // Get the cursor handle
  CURSORINFO ci;
  ci.cbSize = sizeof(CURSORINFO);
  GetCursorInfo(&ci);

  if (ci.hCursor != g_last_cursor)
  {
    BYTE* original_pix = NULL;

    if (!c_show_small_cursor)
    {
      HCURSOR original_cursor = get_cached_system_cursor(ci.hCursor);
      if (original_cursor != NULL)
      {
        ci.hCursor = original_cursor;
      }
    }

    EnterCriticalSection(&g_lock);

    g_last_cursor = ci.hCursor;

    HDC context = GetDC(NULL);
    HDC mem_context = CreateCompatibleDC(context);

    ICONINFO ii;
    auto succeeded = GetIconInfo(ci.hCursor, &ii);

    // Check if the icon is black and white and store the appropriate bmp and scale
    auto is_bw = (ii.hbmColor == NULL);
    HBITMAP original_bmp = is_bw ? ii.hbmMask : ii.hbmColor;
    g_cursor_scale = is_bw ? c_bw_cursor_scale : c_color_cursor_scale;

    if (succeeded)
    {
      // Store the hotspot
      g_hotspot_x = ii.xHotspot;
      g_hotspot_y = ii.yHotspot;

      // Query the icon bitmap's size
      BITMAPINFO original_bmi;
      original_bmi.bmiHeader.biSize = sizeof(original_bmi.bmiHeader);
      HGDIOBJ old_object = SelectObject(mem_context, original_bmp);
      succeeded = GetDIBits(context, original_bmp, 0, 0, NULL, &original_bmi, DIB_RGB_COLORS);

      const auto original_width = original_bmi.bmiHeader.biWidth;
      const auto original_height = is_bw ? original_bmi.bmiHeader.biHeight/2 : original_bmi.bmiHeader.biHeight;

      if (succeeded && original_width <= g_max_cursor_x && original_height <= g_max_cursor_y)
      {
        // Copy the original bitmap to a buffer
        original_pix = (BYTE*)malloc((is_bw ? 8 : 4) * original_width * original_height);
        original_bmi.bmiHeader.biBitCount = 32;
        original_bmi.bmiHeader.biCompression = BI_RGB;
        original_bmi.bmiHeader.biHeight = abs(original_bmi.bmiHeader.biHeight);
        succeeded = GetDIBits(context, original_bmp, 0, original_bmi.bmiHeader.biHeight, original_pix, &original_bmi, DIB_RGB_COLORS);

        if (succeeded)
        {
          // Calculate the dimensions of the big cursor
          const UINT big_width = original_width * g_cursor_scale;
          const UINT big_height = original_height * g_cursor_scale;
          g_width = big_width;
          g_height = big_height;

          // Cleanup the old cursor if one exists
          if (g_cursor_bitmap != NULL)
          {
            if (!DeleteObject(g_cursor_bitmap))
            {
              error("DeleteObject");
            }
          }

          // Create a DIB for the big cursor
          BYTE* big_pix;
          BITMAPINFO big_bmi = {0};
          big_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          big_bmi.bmiHeader.biBitCount = 32;
          big_bmi.bmiHeader.biCompression = BI_RGB;
          big_bmi.bmiHeader.biWidth = big_width;
          big_bmi.bmiHeader.biHeight = big_height;
          big_bmi.bmiHeader.biPlanes = 1;
          g_cursor_bitmap = CreateDIBSection(mem_context, &big_bmi, DIB_RGB_COLORS, (void**)&big_pix, NULL, 0);

          // Generate the big cursor
          for (UINT x=0; x<original_width; ++x)
          {
            for (UINT y=0; y<original_height; ++y)
            {
              auto ooffset = 4*((y*original_width)+x);

              BYTE b = original_pix[ooffset];
              BYTE g = original_pix[ooffset+1];
              BYTE r = original_pix[ooffset+2];

              // For non-transparent pixels, calculate the weighted average of the current color and the desired color
              if ((is_bw && (r != 0 && b != 0 && g != 0)) ||
                  (!is_bw && original_pix[ooffset+3] >= c_transparency_threshold))
              {
                r = (BYTE)((((double)c_desired_r)*c_weight) + (((double)r)*(1.0-c_weight)));
                g = (BYTE)((((double)c_desired_g)*c_weight) + (((double)g)*(1.0-c_weight)));
                b = (BYTE)((((double)c_desired_b)*c_weight) + (((double)b)*(1.0-c_weight)));
              }
              else
              {
                r = 0;
                g = 0;
                b = 0;
              }

              // Add the scaled pixel to the buffer for the big cursor
              for (UINT bx=0; bx < g_cursor_scale; ++bx)
              {
                for (UINT by=0; by < g_cursor_scale; ++by)
                {
                  auto boffset = 4*((((y*g_cursor_scale)+by)*big_width)+(x*g_cursor_scale)+bx);
                  big_pix[boffset] = b;
                  big_pix[boffset+1] = g;
                  big_pix[boffset+2] = r;
                  big_pix[boffset+3] = 255;
                }
              }
            }
          }
        }

        SelectObject(mem_context, old_object);
      }
    }

    // Cleanup buffers for the old cursor
    if (original_pix != NULL)
    {
      free(original_pix);
    }
    DeleteObject(ii.hbmMask);
    if (!is_bw)
    {
      DeleteObject(ii.hbmColor);
    }

    if (!DeleteDC(mem_context))
    {
      error("DeleteDC");
    }

    ReleaseDC(NULL, context);

    LeaveCriticalSection(&g_lock);

    if (g_cursor_window != NULL && succeeded)
    {
      RedrawWindow(g_cursor_window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
  }
}

void move_cursor(UINT x, UINT y)
{
  SetWindowPos(g_cursor_window,
               HWND_TOP,
               ((1.0/c_screen_scale_factor) * x) - (g_hotspot_x*g_cursor_scale),
               ((1.0/c_screen_scale_factor) * y) - (g_hotspot_y*g_cursor_scale),
               g_width,
               g_height,
               0);
}

LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  update_cursor_if_changed();

  if (nCode >= 0 && wParam == WM_MOUSEMOVE)
  {
    MSLLHOOKSTRUCT* mhs = (MSLLHOOKSTRUCT*)lParam;
    move_cursor(mhs->pt.x, mhs->pt.y);
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

  case WM_PAINT:
      {
          EnterCriticalSection(&g_lock);
        
          PAINTSTRUCT ps;
          HDC context = BeginPaint(hwnd, &ps);
          HDC mem_context = CreateCompatibleDC(context);

          BITMAP bitmap;
          HGDIOBJ original = SelectObject(mem_context, g_cursor_bitmap);
          GetObject(g_cursor_bitmap, sizeof(bitmap), &bitmap);
          BitBlt(context, 0, 0, bitmap.bmWidth, bitmap.bmHeight, mem_context, 0, 0, SRCCOPY);

          SelectObject(mem_context, original);
          DeleteDC(mem_context);
          EndPaint(hwnd, &ps);

          LeaveCriticalSection(&g_lock);
      }
      return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void setup_window()
{
  HINSTANCE instance = GetModuleHandle(NULL);

  WNDCLASS wc = {0};
  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = instance;
  wc.lpszClassName = c_win_class;

  RegisterClass(&wc);

  g_cursor_window = CreateWindowEx(
    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
    c_win_class,
    L"GiantCursor",
    WS_DLGFRAME | WS_VISIBLE,
    0, 0, 0, 0,
    NULL,
    NULL,
    instance,
    NULL
  );

  // Hide titlebar and border
  SetWindowLong(g_cursor_window, GWL_STYLE, 0);

  // Enable click through
  LONG current_style = GetWindowLong(g_cursor_window, GWL_EXSTYLE);
  SetWindowLong(g_cursor_window, GWL_EXSTYLE, current_style | WS_EX_TRANSPARENT | WS_EX_LAYERED);

  // Make black pixels transparent:
  SetLayeredWindowAttributes(g_cursor_window, RGB(0,0,0), 0, LWA_COLORKEY);

  ShowWindow(g_cursor_window, SW_SHOWDEFAULT);
}

void load_config()
{
  FILE* fp = fopen("config.txt", "r");
  if (!fp)
  {
    error("opening config");
  }
  UINT r, g, b;
  if (fscanf(fp, "%u %u %u %u %u %lf %u %lf %u",
                  &c_color_cursor_scale,
                  &c_bw_cursor_scale,
                  &r, &g, &b,
                  &c_weight,
                  &c_transparency_threshold,
                  &c_screen_scale_factor,
                  &c_show_small_cursor
                  ) != 9)
  {
    error("parsing config");
  }
  fclose(fp);
  c_desired_r = r;
  c_desired_g = g;
  c_desired_b = b;
}

typedef HRESULT (__cdecl *SetProcessDpiAwareness_t)(UINT); 

void try_enable_auto_scaling()
{
  const auto PROCESS_PER_MONITOR_DPI_AWARE = 2;

  HMODULE lib = LoadLibrary(TEXT("Shcore.dll"));
  if (lib != NULL)
  {
    SetProcessDpiAwareness_t SetProcessDpiAwareness = (SetProcessDpiAwareness_t)GetProcAddress(lib, "SetProcessDpiAwareness");
    if (SetProcessDpiAwareness != NULL)
    {
      if (FAILED(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)))
      {
        error("SetProcessDpiAwareness");
      }
    }

    if (!FreeLibrary(lib))
    {
      error("FreeLibrary");
    }
  }
}

// NB this will leak memory, ensure this is only called a constant number of times
HCURSOR make_transparent_cursor()
{
  HINSTANCE instance = GetModuleHandle(NULL);

  // Make a transparent cursor
  auto mask_size = (g_max_cursor_x*g_max_cursor_y)/8;
  BYTE* buf = (BYTE*)malloc(2*mask_size);
  memset(buf, 0xFF, mask_size); // AND mask
  memset(buf + mask_size, 0x00, mask_size); // XOR mask
  HCURSOR cursor = CreateCursor(instance, 0, 0, g_max_cursor_x, g_max_cursor_y, buf, buf + mask_size);
  if (cursor == NULL)
  {
    error("creating cursor");
  }

  return cursor;
}

void cache_and_hide_system_cursors()
{
  const auto count = sizeof(g_cached_system_cursors)/sizeof(cached_cursor_t);
  for (UINT i = 0; i < count; ++i)
  {
    // Load and store the original cursor
    DWORD id = g_cached_system_cursors[i].id;
    HCURSOR original = LoadCursor(NULL, MAKEINTRESOURCE(id));
    if (original == NULL)
    {
      // Ignore errors from obsolete cursors
      g_cached_system_cursors[i].original_cursor = INVALID_HANDLE_VALUE;
      continue;
    }
    g_cached_system_cursors[i].original_cursor = original;

    // Store a copy in the cache
    HCURSOR copy = CopyIcon(original);
    if (copy == NULL)
    {
      error("CopyIcon");
    }
    g_cached_system_cursors[i].cached_cursor = copy;

    // Make a unique transparent cursor
    HCURSOR transparent = make_transparent_cursor();

    // Replace the system cursor with the transparent one
    if (!SetSystemCursor(transparent, id))
    {
      error("SetSystemCursor");
    }
  }
}

void clear_cache()
{
  const auto count = sizeof(g_cached_system_cursors)/sizeof(cached_cursor_t);
  for (UINT i = 0; i < count; ++i)
  {
    DestroyCursor(g_cached_system_cursors[i].cached_cursor);
  }
}

void main_inner()
{
  try_enable_auto_scaling();

  load_config();

  InitializeCriticalSectionAndSpinCount(&g_lock, 0x00000400);

  g_max_cursor_x = GetSystemMetrics(SM_CXCURSOR);
  g_max_cursor_y = GetSystemMetrics(SM_CYCURSOR);

  if (!c_show_small_cursor)
  {
    cache_and_hide_system_cursors();
  }

  // Create the window to display the big cursor
  setup_window();

  // Setup the mouse hook so the big cursor will move when the mouse moves
  HHOOK hook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)&mouseProc, GetModuleHandle(NULL), 0);

  // Paint the big cursor
  update_cursor_if_changed();

  // Move the big cursor into view
  CURSORINFO ci;
  ci.cbSize = sizeof(CURSORINFO);
  GetCursorInfo(&ci);
  move_cursor(ci.ptScreenPos.x, ci.ptScreenPos.y);

  MSG message;
  while (GetMessage(&message,NULL,0,0))
  {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  UnhookWindowsHookEx(hook);

  if (!c_show_small_cursor)
  {
    clear_cache();
  }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  main_inner();
  return 0;
}

int main()
{
  main_inner();
  return 0;
}
