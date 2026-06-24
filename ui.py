import os
import subprocess
import tkinter as tk
from tkinter import messagebox
import customtkinter as ctk
import os
import sys

def get_resource_path(relative_path):
    """ Получает абсолютный путь к ресурсам (работает и для dev, и для PyInstaller) """
    if hasattr(sys, '_MEIPASS'):
        # Путь к временной папке PyInstaller
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("."), relative_path)

# Получаем правильный путь к Syring.exe
syring_path = get_resource_path("Syring.exe")

# Теперь можно использовать этот путь, например, для запуска:
# os.system(syring_path) или subprocess.run([syring_path])
print(f"Путь к файлу: {syring_path}")
# Appearance and Theme settings (Dark mode with Blue accents)
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")


class InjectorApp(ctk.CTk):

    def __init__(self):
        super().__init__()

        self.title("Stormworks Camera Mod Injector")
        self.geometry("450x280")
        self.resizable(False, False)

        # Base filenames
        self.injector_exe = "syring.exe"
        self.dll_name = "Cameramod.dll"

        self.create_widgets()

    def create_widgets(self):
        # Header Title
        self.title_label = ctk.CTkLabel(
            self,
            text="Stormworks Multi-Window Mod",
            font=ctk.CTkFont(size=20, weight="bold"),
        )
        self.title_label.pack(pady=(20, 10))

        # Subtitle / Hint
        self.subtitle_label = ctk.CTkLabel(
            self,
            text="Make sure the game is running before injecting",
            font=ctk.CTkFont(size=12, slant="italic"),
            text_color="gray",
        )
        self.subtitle_label.pack(pady=(0, 20))

        # Process Target Input Frame
        self.input_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.input_frame.pack(pady=10, padx=20, fill="x")

        self.proc_label = ctk.CTkLabel(
            self.input_frame, text="Game Process:", font=ctk.CTkFont(size=14)
        )
        self.proc_label.pack(side="left", padx=(0, 10))

        # Entry field with default Stormworks executable name
        self.process_entry = ctk.CTkEntry(
            self.input_frame, placeholder_text="stormworks64.exe"
        )
        self.process_entry.insert(0, "stormworks64.exe")
        self.process_entry.pack(side="left", fill="both", expand=True)

        # Main Action Button
        self.start_button = ctk.CTkButton(
            self,
            text="START MOD",
            font=ctk.CTkFont(size=16, weight="bold"),
            height=45,
            command=self.inject_mod,
        )
        self.start_button.pack(pady=(20, 10), padx=40, fill="x")

        # Bottom Status Bar
        self.status_label = ctk.CTkLabel(
            self, text="Status: Idle", text_color="yellow"
        )
        self.status_label.pack(pady=(5, 10))

    def inject_mod(self):
        process_name = self.process_entry.get().strip()

        if not process_name:
            messagebox.showerror("Error", "Process name cannot be empty!")
            return

        # 1. Автоматически получаем абсолютный путь к папке со скриптом ui.py
        script_dir = os.path.dirname(os.path.abspath(__file__))

        # 2. Собираем ЖЕСТКИЕ абсолютные пути для инжектора и DLL
        full_injector_path = os.path.join(script_dir, self.injector_exe)
        full_dll_path = os.path.join(script_dir, self.dll_name)

        # Проверяем, лежат ли файлы рядом с ui.py
        if not os.path.exists(full_injector_path):
            messagebox.showerror(
                "Error", f"Injector not found at:\n{full_injector_path}"
            )
            return
        if not os.path.exists(full_dll_path):
            messagebox.showerror(
                "Error", f"DLL not found at:\n{full_dll_path}"
            )
            return

        self.status_label.configure(text="Injecting...", text_color="lightblue")
        self.update_idletasks()

        # 3. Формируем команду с ПОЛНЫМИ путями, прямо как в твоем успешном тесте
        # Результат будет: ["D:\...\syring.exe", "D:\...\Cameramod.dll", "stormworks64.exe"]
        cmd = [full_injector_path, full_dll_path, process_name]

        try:
            # Запускаем процесс в контексте его родной папки
            result = subprocess.run(
                cmd,
                cwd=script_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )

            # Проверяем код возврата syring.exe (обычно 0 — успех)
            if result.returncode == 0:
                self.status_label.configure(
                    text="Mod successfully injected!", text_color="green"
                )
                messagebox.showinfo(
                    "Success",
                    "Mod injected successfully! Please check your game window.",
                )
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                self.status_label.configure(
                    text="Injection Failed", text_color="red"
                )
                messagebox.showerror(
                    "Injector Error",
                    f"The injector returned an error:\n{error_msg}",
                )

        except Exception as e:
            self.status_label.configure(text="Error", text_color="red")
            messagebox.showerror(
                "Critical Error", f"Failed to run the injector:\n{str(e)}"
            )


if __name__ == "__main__":
    app = InjectorApp()
    app.mainloop()