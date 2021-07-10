@echo off
cd src
find2 ../../libs/allegro-4.2.2-xc/ ../libresample/ ../ymfm/ | ctags -L -
cd ..
