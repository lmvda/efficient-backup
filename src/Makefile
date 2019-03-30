 
TP: servidor.c cliente.c
	@ gcc -Wall -O2 -o sobusrv servidor.c
	@ gcc -Wall -O2 -o sobucli cliente.c

install:
	@ mkdir ~/.Backup
	@ mkdir ~/.Backup/data
	@ mkdir ~/.Backup/metadata
	@ sudo mv sobusrv sobucli /usr/local/bin


start:
	@ sobusrv &

stop:
	@ sobucli quit

clean:
	@ rm -rf ~/.Backup
	@ rm -f sobucli sobusrv erros.txt fifoComandos teste2.txt teste3.txt
	@ sudo rm -f /usr/local/bin/sobusrv /usr/local/bin/sobucli


test:
	@ echo "INICIO TESTE"
	@ make clean
	@ cp teste.txt teste2.txt
	@ cp teste.txt teste3.txt
	@ make TP
	@ make install
	@ echo "INICIAR SERVIDOR"
	@ make start
	@ sleep 1
	@ echo "BACKUP 1"
	@ sobucli backup teste2.txt
	@ sleep 1
	@ echo "BACKUP 2"
	@ sobucli backup teste3.txt
	@ sleep 1
	@ rm -f teste2.txt teste3.txt
	@ echo "RESTORE"
	@ sobucli restore teste3.txt
	@ sleep 1
	@ echo "PARAR SERVIDOR"
	@ sobucli quit
	@ echo "FIM TESTE"


test_delete:
	@ echo "INICIO TESTE"
	@ make clean
	@ cp teste.txt teste2.txt
	@ make TP
	@ make install
	@ echo "INICIAR SERVIDOR"
	@ make start
	@ sleep 1
	@ echo "BACKUP"
	@ sobucli backup teste2.txt
	@ sleep 1
	@ rm -f teste2.txt
	@ echo "RESTORE"
	@ sobucli restore teste2.txt
	@ sleep 1
	@ rm -f teste2.txt
	@ echo "DELETE"
	@ sobucli delete teste2.txt
	@ sleep 1
	@ echo "RESTORE 2 (deve falhar)"
	@ sobucli restore teste2.txt
	@ sleep 1
	@ echo "PARAR SERVIDOR"
	@ sobucli quit
	@ echo "FIM TESTE"

test_gc:
	@ echo "INICIO TESTE"
	@ make clean
	@ cp teste.txt teste2.txt
	@ cp teste.txt teste3.txt
	@ make TP
	@ make install
	@ echo "INICIAR SERVIDOR"
	@ make start
	@ sleep 1
	@ echo "BACKUP"
	@ sobucli backup teste2.txt teste3.txt teste10.txt
	@ sleep 1
	@ rm -f teste2.txt
	@ echo "DELETE"
	@ sobucli delete teste2.txt teste3.txt
	@ sleep 1
	@ echo "GC"
	@ sobucli gc
	@ sleep 1
	@ echo "PARAR SERVIDOR"
	@ sobucli quit
	@ echo "FIM TESTE"
