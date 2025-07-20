CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99
LDFLAGS = -lpthread -lrt
TARGET = rt_demo
SOURCE = rt_demo.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +s /usr/local/bin/$(TARGET)

test: $(TARGET)
	@echo "Rulare test de 30 secunde..."
	@echo "NOTĂ: Pentru rezultate optime, rulează cu sudo pentru privilegii RT"
	sudo ./$(TARGET) 30

help:
	@echo "Comenzi disponibile:"
	@echo "  make         - Compilează aplicația"
	@echo "  make clean   - Șterge fișierele compilate"
	@echo "  make install - Instalează în /usr/local/bin cu setuid"
	@echo "  make test    - Rulează un test de 30 secunde"
	@echo "  make help    - Afișează acest mesaj"
	@echo ""
	@echo "Utilizare:"
	@echo "  ./rt_demo [durata_in_secunde]"
	@echo "  sudo ./rt_demo 60"
