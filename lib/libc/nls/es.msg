$set 1
$ EPERM
1 Operación no permitida
$ ENOENT
2 Archivo o directorio inexistente
$ ESRCH
3 Proceso inexistente
$ EINTR
4 Llamada del sistema interrumpida
$ EIO
5 Error de E/S
$ ENXIO
6 Dispositivo no configurado
$ E2BIG
7 La lista de argumentos es demasiado larga
$ ENOEXEC
8 Error en el formato del ejecutable 
$ EBADF
9 Descriptor de archivo incorrecto
$ ECHILD
10 No hay procesos hijo 
$ EDEADLK
11 Se ha evitado bloqueo del recurso
$ ENOMEM
12 No se pudo asignar la memoria pedida
$ EACCES
13 Permiso denegado
$ EFAULT
14 Dirección incorrecta
$ ENOTBLK
15 Se necesita un dispositivo de bloques
$ EBUSY
16 Dispositivo ocupado
$ EEXIST
17 El archivo ya existe
$ EXDEV
18 Enlace entre dispositivos
$ ENODEV
19 Operación inadecuada para este dispositivo
$ ENOTDIR
20 No es un directorio
$ EISDIR
21 Es un directorio
$ EINVAL
22 Argumento inadecuado
$ ENFILE
23 Hay demasiados archivos abiertos en el sistema
$ EMFILE
24 Hay demasiados archivos abiertos
$ ENOTTY
25 IOCTL inapropiado para el dispositivo
$ ETXTBSY
26 Archivo de texto ocupado
$ EFBIG
27 Archivo demasiado grande
$ ENOSPC
28 No queda espacio libre en el dispositivo
$ ESPIPE
29 Búsqueda ilegal
$ EROFS
30 Archivo del sistema de sólo lectura
$ EMLINK
31 Demasiados enlaces
$ EPIPE
32 Canal (pipe) roto
$ EDOM
33 El argumento numérico está fuera de rango
$ ERANGE
34 El resultado es demasiado grande
$ EAGAIN, EWOULDBLOCK
35 El recurso no está disponible temporalmente
$ EINPROGRESS
36 Operación actualmente en proceso
$ EALREADY
37 La operación ya estaba realizándose
$ ENOTSOCK
38 Operación de socket inaceptable por el dispositivo
$ EDESTADDRREQ
39 Se necesita una dirección de destino
$ EMSGSIZE
40 Mensaje demasiado largo
$ EPROTOTYPE
41 Tipo erróneo de protocolo para el socket
$ ENOPROTOOPT
42 Protocolo no disponible
$ EPROTONOSUPPORT
43 Protocolo no contemplado
$ ESOCKTNOSUPPORT
44 Tipo de socket no contemplado
$ EOPNOTSUPP
45 Operación no contemplada
$ EPFNOSUPPORT
46 Familia de protocolos no contemplada
$ EAFNOSUPPORT
47 Familia de direcciones no contemplada por la familia de protocolos
$ EADDRINUSE
48 La dirección ya está en uso
$ EADDRNOTAVAIL
49 No se pudo asignar la dirección pedida
$ ENETDOWN
50 La red no funciona
$ ENETUNREACH
51 No se puede acceder a la red
$ ENETRESET
52 La conexión a la red se interrumpió al reinicializar
$ ECONNABORTED
53 La conexión se ha abortado debido a problemas en el software
$ ECONNRESET
54 El interlocutor ha reinicializado la comunicación
$ ENOBUFS
55 No queda espacio en el buffer
$ EISCONN
56 El socket ya estaba conectado
$ ENOTCONN
57 El socket no está conectado
$ ESHUTDOWN
58 No se puede enviar tras la desconexión del socket
$ ETOOMANYREFS
59 Demasiadas referencias: no se pueden unir
$ ETIMEDOUT
60 El tiempo de conexión ha expirado
$ ECONNREFUSED
61 Conexión rehusada
$ ELOOP
62 Demasiados niveles de enlaces simbólicos
$ ENAMETOOLONG
63 Nombre de archivo demasiado largo
$ EHOSTDOWN
64 La máquina está fuera de servicio
$ EHOSTUNREACH
65 No hay ruta hasta la máquina
$ ENOTEMPTY
66 Directorio no vacío
$ EPROCLIM
67 Demasiados procesos
$ EUSERS
68 Demasiados usuarios
$ EDQUOT
69 Cuota de disco sobrepasada
$ ESTALE
70 Descriptor de archivo NFS inválido
$ EREMOTE
71 Ruta con demasiados niveles
$ EBADRPC
72 La estructura de la RPC es errónea
$ ERPCMISMATCH
73 La versión de la RPC es errónea
$ EPROGUNAVAIL
74 La RPC no está accesible
$ EPROGMISMATCH
75 Versión errónea del programa
$ EPROCUNAVAIL
76 Procedimiento erróneo para el programa
$ ENOLCK
77 No hay bloqueos disponibles
$ ENOSYS
78 Función no realizada
$ EFTYPE
79 Tipo de archivo o formato inapropiado
$ EAUTH
80 Error de la autentificación
$ ENEEDAUTH
81 Se necesita un autenticador
$ EIDRM
82 Identificador quitado
$ ENOMSG
83 No hay mensajes del tipo deseado
$ EOVERFLOW
84 Valor demasiado grande para ser almacenado en el tipo de datos
$ EILSEQ
85 Secuencia de bytes no legal
$ ENOTSUP
86 No soportado
$ ECANCELED
87 Operación cancelada
$ EBADMSG
88 Mensaje incorrecto o corrupto
$ ENODATA
89 No hay mensajes disponibles
$ ENOSR
90 No hay recursos de tipo STREAM
$ ENOSTR
91 No es un STREAM
$ ETIME
92 Tiempo de espera agotado en el ioctl STREAM
$ ENOATTR
93 Atributo no encontrado
$ EMULTIHOP
94 Se ha intentado un multisalto
$ ENOLINK
95 El enlace se ha servido
$ EPROTO
96 Error de protocolo
$set 2
$ SIGHUP
1 Fín de línea (hangup)
$ SIGINT
2 Interrumpido
$ SIGQUIT
3 Terminado
$ SIGILL
4 Instrucción ilegal
$ SIGTRAP
5 Depuración
$ SIGABRT
6 Llamada de aborto
$ SIGEMT
7 Captura de EMT
$ SIGFPE
8 Excepción de coma flotante
$ SIGKILL
9 Matado
$ SIGBUS
10 Error en el bus
$ SIGSEGV
11 Fallo de segmentación
$ SIGSYS
12 Llamada al sistema errónea
$ SIGPIPE
13 Canal (pipe) roto
$ SIGALRM
14 Alarma del reloj
$ SIGTERM
15 Terminado
$ SIGURG
16 Condición urgente de E/S
$ SIGSTOP
17 Detenido (señal)
$ SIGTSTP
18 Detenido
$ SIGCONT
19 Continuando
$ SIGCHLD
20 Proceso hijo finalizado
$ SIGTTIN
21 Detenido (entrada tty)
$ SIGTTOU
22 Detenido (salida tty)
$ SIGIO
23 E/S posible
$ SIGXCPU
24 Se ha sobrepasado el tiempo límite de la CPU
$ SIGXFSZ
25 Se ha sobrepasado el límite de longitud de archivo
$ SIGVTALRM
26 Temporizador virtual expirado
$ SIGPROF
27 Temporizador de perfilación expirado
$ SIGWINCH
28 Cambios en el tamaño de la ventana
$ SIGINFO
29 Petición de información
$ SIGUSR1
30 Señal definida por el usuario nº 1
$ SIGUSR2
31 Señal definida por el usuario nº 2
$ SIGPWR
32 Fallo o reinicio de la alimentación
