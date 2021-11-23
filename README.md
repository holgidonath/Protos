# TPE 2 Protos
## POP3 Proxy Server

Los archivos fuente están ubicados dentro de la carpeta TP2/src.

### Generar versión ejecutable
`cd TP2/src`

`make`

y esto genera dentro de la misma carpeta de los archivos fuente los binarios `pop3filter` (ejecutable del proxy) y `pop3ctl` (ejectuable del cliente de administrador)

### Modo de uso
#### pop3filter
Se debe ejecutar el comando `./pop3filter [opciones] [dirección donde montar]`

Donde en la dirección se debe especificar aquella donde se quiere montar el servidor.
El ejecutable soporta las siguientes opciones:

`-e <error-file>`: Configura el archivo de error al cual se redirije stderr. El valor por defecto es /dev/null.


`-h`: Imprime la ayuda.


`-l <pop3-address>`: Configura la dirección donde va a escuchar el proxy. Por defecto escucha en todas las interfaces.


`-L <management-address>`: Configura la dirección en la cual el administrador va a escuchar. Por defecto usa loopback.


`-o <management-port>`: Configura el puerto para el administrador. Por defecto es 9090.


`-p <local-port>`: Configura el puerto TCP para conexiones provenientes de los clientes para conexiones POP3. Por defecto el puerto es 1110.


`-P <origin-port>`: Configura el puerto TCP de origen para el servidor origen POP3. Por defecto el puerto es 110.


`-t <cmd>`: Configura el comando externo para filtrar.


`-v`: Imprime información asociada a la versión del proxy.

#### pop3ctl
Se debe ejecutar el comando `./pop3ctl [opciones]`
El ejecutable soporta las siguientes opciones:
`-h`: Imprime la ayuda.

`-L <management-address>`: Configura la dirección en la cual el administrador va a escuchar. Por defecto usa loopback.

`-o <management-port>`: Configura el puerto al cual el administrador se conecta. Por defecto es 9090.

