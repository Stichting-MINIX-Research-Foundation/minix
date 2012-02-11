$set 1
$ EPERM
1 Operacja nie dozwolona
$ ENOENT
2 Nie ma takiego pliku ani katalogu
$ ESRCH
3 Nie ma takiego procesu
$ EINTR
4 Przerwane wywo³anie systemowe
$ EIO
5 B³±d wej¶cia/wyj¶cia
$ ENXIO
6 Urz±dzenie nie skonfigurowane
$ E2BIG
7 Lista argumentów jest za d³uga
$ ENOEXEC
8 B³êdny format pliku wykonywalnego
$ EBADF
9 Z³y deskryptor pliku
$ ECHILD
10 Brak procesów potomnych
$ EDEADLK
11 Unikniêto zakleszczenia zasobów
$ ENOMEM
12 Brak pamiêci do przydzia³u
$ EACCES
13 Brak dostêpu
$ EFAULT
14 Z³y adres
$ ENOTBLK
15 Wymagane urz±dzenie blokowe
$ EBUSY
16 Urz±dzenie zajête
$ EEXIST
17 Plik istnieje
$ EXDEV
18 Powi±zanie miedzy urz±dzeniami
$ ENODEV
19 Operacja nie obs³ugiwana przez urz±dzenie
$ ENOTDIR
20 To nie jest katalog
$ EISDIR
21 To jest katalog
$ EINVAL
22 B³êdny argument
$ ENFILE
23 Za du¿o otwrtych plików w systemie
$ EMFILE
24 Za du¿o otwartych plików
$ ENOTTY
25 Niew³a¶ciwy dostêp do urz±dzenia
$ ETXTBSY
26 Plik tekstowy zajêty
$ EFBIG
27 Plik zbyt du¿y
$ ENOSPC
28 Nie ma miejsca na urz±dzeniu
$ ESPIPE
29 B³êdne przesuniêcie
$ EROFS
30 System plików tylko do odczytu
$ EMLINK
31 Za du¿o linków
$ EPIPE
32 Przerwany potok
$ EDOM
33 Argument liczbowy spoza zakresu
$ ERANGE
34 Za du¿y wynik
$ EAGAIN, EWOULDBLOCK
35 Zasoby chwilowo niedostêpne
$ EINPROGRESS
36 Operacja jest w³a¶nie wykonywana
$ EALREADY
37 Operacja jest ju¿ wykonywana
$ ENOTSOCK
38 Operacja na obiekcie, który nie jest gniazdem
$ EDESTADDRREQ
39 Wymagany adres przeznaczenia
$ EMSGSIZE
40 Wiadomo¶æ za d³uga
$ EPROTOTYPE
41 Ty protoko³u nie pasuje do gniazda
$ ENOPROTOOPT
42 Protocó³ nie jest dostêpny
$ EPROTONOSUPPORT
43 Protocó³ nie jest obs³ugiwany
$ ESOCKTNOSUPPORT
44 Nie obs³ugiwany typ gniazda
$ EOPNOTSUPP
45 Operacja nie obs³ugiwana
$ EPFNOSUPPORT
46 Nie obs³ugiwana rodzina protoko³ów
$ EAFNOSUPPORT
47 Rodzina adresów nie obs³ugiwana przez rodzinê protoko³ów
$ EADDRINUSE
48 Adres jest aktualnie w u¿yciu
$ EADDRNOTAVAIL
49 Nie mo¿na przypisaæ ¿±danego adresu
$ ENETDOWN
50 Sieæ nie dzia³a
$ ENETUNREACH
51 Sieæ jest niedostêpna
$ ENETRESET
52 Sieæ przerwa³a po³±czenie po resecie
$ ECONNABORTED
53 Oprogramowanie spowodowa³o przerwanie po³±czenia
$ ECONNRESET
54 Po³±czenie zerwane przez drug± stronê
$ ENOBUFS
55 Brak miejsca w buforze
$ EISCONN
56 Gniazdo jest ju¿ po³±czone
$ ENOTCONN
57 Gniazdo nie jest po³±czone
$ ESHUTDOWN
58 Nie mo¿na wysy³aæ po zamkniêciu gniazda
$ ETOOMANYREFS
59 Za du¿o powi±zañ: dowi±zanie nie mo¿liwe
$ ETIMEDOUT
60 Przekroczono czas oczekiwania na po³±czenie
$ ECONNREFUSED
61 Po³±czenie odrzucone
$ ELOOP
62 Za du¿o wzajemnych symlinków
$ ENAMETOOLONG
63 Zbyt d³uga nazwa pliku
$ EHOSTDOWN
64 Host jest wy³±czony
$ EHOSTUNREACH
65 Brak drogi do hosta
$ ENOTEMPTY
66 Katalog nie jest pusty
$ EPROCLIM
67 Za du¿o procesów
$ EUSERS
68 Zbyt wielu u¿ytkowników
$ EDQUOT
69 Przekroczono limit miejsca na dysku
$ ESTALE
70 Uchwyt pliku NFS jest nieaktualny
$ EREMOTE
71 Za du¿o poziomów zagnie¿d¿enia w ¶cie¿ce
$ EBADRPC
72 RPC b³êdna struktura
$ ERPCMISMATCH
73 RPC z³a wersja
$ EPROGUNAVAIL
74 RPC program niedostêpny
$ EPROGMISMATCH
75 Z³a wersja programu
$ EPROCUNAVAIL
76 Z³a procedura dla programu
$ ENOLCK
77 Blokady nie s± dostêpne
$ ENOSYS
78 Niezaimplementowana funkcja
$ EFTYPE
79 Niew³a¶ciwy typ lub format pliku
$ EAUTH
80 B³±d uwierzytelnienia
$ ENEEDAUTH
81 Wymagane uwierzytelnienie
$ EIDRM
82 Identyfikator zosta³ usuniêty
$ ENOMSG
83 Brak komunikatu po¿adanego typu
$ EOVERFLOW
84 Warto¶æ za du¿a aby zapamiêtaæ j± w zdefiniowanym typie danych
$ EILSEQ
85 B³êdna sekwencja bajtów
$set 2
$ SIGHUP
1 Roz³±czenie
$ SIGINT
2 Przerwanie
$ SIGQUIT
3 Wyj¶cie
$ SIGILL
4 Nieznana instrukcja
$ SIGTRAP
5 Pu³apka debuggera/BPT
$ SIGABRT
6 Przerwanana pu³apka
$ SIGEMT
7 Pu³apka EMT
$ SIGFPE
8 B³±d w obliczeniach zmiennoprzecinkowych
$ SIGKILL
9 Unicestwiony
$ SIGBUS
10 B³±d szyny
$ SIGSEGV
11 Naruszenie ochrony pamiêci
$ SIGSYS
12 B³êdne wywo³anie systemowe
$ SIGPIPE
13 Przerwany potok
$ SIGALRM
14 Budzik
$ SIGTERM
15 Zakoñczony
$ SIGURG
16 Nag³y wypadek I/O
$ SIGSTOP
17 Zatrzymany (sygna³)
$ SIGTSTP
18 Zatrzymany
$ SIGCONT
19 Kontynuacja
$ SIGCHLD
20 Proces potomny zakoñczy³ pracê
$ SIGTTIN
21 Zatrzymany (wej¶cie z tty)
$ SIGTTOU
22 Zatrzymany (wyj¶cie z tty)
$ SIGIO
23 Wej/Wyj dozwolone
$ SIGXCPU
24 Przekroczony limit czasu procesora
$ SIGXFSZ
25 przekroczony limit wielko¶ci pliku
$ SIGVTALRM
26 Wirtualny stoper wyczerpany
$ SIGPROF
27 Koniec stopera profiluj±cego
$ SIGWINCH
28 Zmiana rozmiaru okna
$ SIGINFO
29 ¯±danie infromacji
$ SIGUSR1
30 Sygna³ u¿ytkownika 1
$ SIGUSR2
31 Sygna³ u¿ytkownika 2
$ SIGPWR
32 Awaria zasilania/restart
