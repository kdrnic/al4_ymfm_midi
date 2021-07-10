del *.png
for %%f in (*.wav) do ..\..\Utilities\sox-14.4.2-win32\sox-14.4.2\sox.exe %%f -n spectrogram && ren spectrogram.png %%f.png