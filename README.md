# tekstitv_tekstiksi


purkaa live transport streamista teksti-tv-lähetyksen hakemistoon /tmp/vtx/1/txt tekstitiedostoiksi

tiedostossa
    livetesti määritellään
kanava muodossa
    kanava=("http://192.168.1.12:8001/1:0:16:1D83:1B:46:E080000:0:0:0:" "6751") #svt1 norrbotten
jossa ensiksi on enigma2-digiboksin url kyseiselle kanavalle ja sitä seuraa teksti-tv:n pid (jonka voi selvittää mediainfolla tai vlc:llä)

Oletuksena skripti lukee lähetystä 60 sekuntia ja purkaa sitä samaan aikaan /tmp/vtx/1/txt hakemistoon luettavaan muotoon.
