import os, tkinter, tkinter.colorchooser, subprocess, ctypes, platform

CONFIG_MSG = '\n# Use the control panel to update this file\n# Do not edit by hand\n'
BGCOLOR = 'white'
FONT = ('Arial', 25)

def load_config():
  global config
  try:
    with open('config.txt', 'r') as f:
      inp = f.readline().split()
    config = {
      'color_cursor_scale': int(inp[0]),
      'bw_cursor_scale': int(inp[1]),
      'color': tuple(map(int, inp[2:5])),
      'weight': float(inp[5]),
      'transparency_threshold': int(inp[6]),
      'screen_scale': float(inp[7]),
      'show_small_cursor': int(inp[8]),
    }
  except (FileNotFoundError, IndexError):
    config = {
      'color_cursor_scale': 2,
      'bw_cursor_scale': 2,
      'color': (255, 0, 0),
      'weight': 0.7,
      'transparency_threshold': 220,
      'screen_scale': 1.0,
      'show_small_cursor': 1,
    }

def save_config():
  cfg = ' '.join(map(str, 
    [
      config['color_cursor_scale'],
      config['bw_cursor_scale'],
    ] +
    list(config['color']) +
    [
      config['weight'],
      config['transparency_threshold'],
      config['screen_scale'],
      int(config['show_small_cursor']),
    ]
  ))
  with open('config.txt', 'w') as f:
    f.write(cfg+CONFIG_MSG)

def change_color():
  color = tkinter.colorchooser.askcolor()
  if color[0]:
    color = tuple(map(round, color[0]))
    config['color'] = color

def update_weight(value):
  config['weight'] = int(value)/100

def update_color_cursor_scale(value):
  config['color_cursor_scale'] = int(value)

def update_bw_cursor_scale(value):
  config['bw_cursor_scale'] = int(value)

def update_transparency_threshold(value):
  config['transparency_threshold'] = int(255*int(value)/100)

def update_screen_scale(value):
  config['screen_scale'] = float(value)/100

def update_checkbutton_text(button, value):
  if float(platform.uname().release) >= 5:
    text = '\u2611' if value else '\u2610'
  else:
    text = '[X]' if value else '[  ]'
  button.configure(text=text)

def tap_hide(button):
  config['show_small_cursor'] = not config['show_small_cursor']
  update_checkbutton_text(button, config['show_small_cursor'])

def close():
  root.destroy()

def get_exe_name():
  is_64_bit = os.environ.get('ProgramW6432') is not None
  return 'giant_cursor64.exe' if is_64_bit else 'giant_cursor32.exe'

def stop():
  subprocess.run(['taskkill', '/im', get_exe_name()])
  SPI_SETCURSORS = 0x0057
  if not ctypes.windll.user32.SystemParametersInfoA(SPI_SETCURSORS, 0, 0, 0):
    raise ctypes.WinError()

def restart():
  stop()
  subprocess.Popen(get_exe_name(), start_new_session=True)

def apply():
  save_config()
  restart()

def ok():
  save_config()
  restart()
  close()
  
def make_root():
  global root
  root = tkinter.Tk()
  root.title('GiantCursor Control Panel')
  root.iconbitmap('control_panel.ico')
  root.configure(bg=BGCOLOR)

  color_button = tkinter.Button(root,
                                text='Change Color',
                                font=FONT,
                                command=change_color)
  color_button.grid(row=0, columnspan=3, padx=10, pady=10)

  weight_label = tkinter.Label(root, text='Color Weight', font=FONT, bg=BGCOLOR)
  weight_label.grid(row=1, column=0, pady=4, padx = 4)
  weight_slider = tkinter.Scale(root,
                               from_=0, to=100,
                               orient=tkinter.HORIZONTAL,
                               bg=BGCOLOR,
                               length=300,
                               command=update_weight)
  weight_slider.set(round(config['weight']*100))
  weight_slider.grid(row=1, column=1, columnspan=2, padx=10, pady=10)
  
  transparency_label = tkinter.Label(root, text='Transparency Threshold', font=FONT, bg=BGCOLOR)
  transparency_label.grid(row=2, column=0, pady=4, padx = 4)
  transparency_slider = tkinter.Scale(root,
                                      from_=0, to=100,
                                      orient=tkinter.HORIZONTAL,
                                      bg=BGCOLOR,
                                      length=300,
                                      command=update_transparency_threshold)
  transparency_slider.set(round(config['transparency_threshold']*100/255))
  transparency_slider.grid(row=2, column=1, columnspan=2, padx=10, pady=10)

  color_scale_label = tkinter.Label(root, text='Color Cursor Size', font=FONT, bg=BGCOLOR)
  color_scale_label.grid(row=3, column=0, pady=4, padx = 4)
  color_scale_slider = tkinter.Scale(root,
                                     from_=2, to=30,
                                     orient=tkinter.HORIZONTAL,
                                     bg=BGCOLOR,
                                     length=300,
                                     command=update_color_cursor_scale)
  color_scale_slider.set(config['color_cursor_scale'])
  color_scale_slider.grid(row=3, column=1, columnspan=2, padx=10, pady=10)

  bw_scale_label = tkinter.Label(root, text='Black & White Cursor Size', font=FONT, bg=BGCOLOR)
  bw_scale_label.grid(row=4, column=0, pady=4, padx = 4)
  bw_scale_slider = tkinter.Scale(root,
                                  from_=2, to=30,
                                  orient=tkinter.HORIZONTAL,
                                  bg=BGCOLOR,
                                  length=300,
                                  command=update_bw_cursor_scale)
  bw_scale_slider.set(config['color_cursor_scale'])
  bw_scale_slider.grid(row=4, column=1, columnspan=2, padx=10, pady=10)

  scale_label = tkinter.Label(root, text='Screen Scale', font=FONT, bg=BGCOLOR)
  scale_label.grid(row=5, column=0, pady=4, padx=4)
  scale_slider = tkinter.Scale(root,
                               from_=100, to=175, resolution=25,
                               orient=tkinter.HORIZONTAL,
                               bg=BGCOLOR,
                               length=300,
                               command=update_screen_scale)
  scale_slider.set(config['screen_scale']*100)
  scale_slider.grid(row=5, column=1, columnspan=2, padx=10, pady=10)

  hide_label = tkinter.Label(root, text='Show Small Cursor', font=FONT, bg=BGCOLOR)
  hide_label.grid(row=6, column=0, pady=4, padx=4)

  hide_checkbutton = tkinter.Button(root,
                                    font=FONT,
                                    bg=BGCOLOR)
  hide_checkbutton.configure(command=lambda: tap_hide(hide_checkbutton))
  update_checkbutton_text(hide_checkbutton, config['show_small_cursor'])
  hide_checkbutton.grid(row=6, column=1, pady=4, padx=4)

  stop_button = tkinter.Button(root,
                                text='Stop',
                                font=FONT,
                                command=stop)
  stop_button.grid(row=7, column=0, padx=10, pady=(60,10))

  restart_button = tkinter.Button(root,
                                text='Restart',
                                font=FONT,
                                command=restart)
  restart_button.grid(row=7, column=1, padx=10, pady=(60,10))

  color_button = tkinter.Button(root,
                                text='Cancel',
                                font=FONT,
                                command=close)
  color_button.grid(row=8, column=0, padx=10, pady=10)
  
  color_button = tkinter.Button(root,
                                text='Apply',
                                font=FONT,
                                command=apply)
  color_button.grid(row=8, column=1, padx=10, pady=10)

  color_button = tkinter.Button(root,
                                text='OK',
                                font=FONT,
                                command=ok)
  color_button.grid(row=8, column=2, padx=10, pady=10)

def main():
  try:
    ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(u'GiantCursor.ControlPanel')
  except AttributeError:
    pass

  load_config()
  make_root()
  root.mainloop()

if __name__ == '__main__':
  main()
