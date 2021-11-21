Si está instalado socat (fijarse con `man socat`) ejecutar

`sudo socat TCP4-LISTEN:9091,crlf,reuseaddr SYSTEM:'[directory]',pty,echo=0`, donde [directory] es la ubicación del archivo pop3.awk

Y en el proxy usar el puerto 9091.

Si tira permisos de error, darle permisos de ejecución a pop3.awk (chmod +x pop3.awk)

Si no está instalado, seguir el siguiente procedimiento:

`wget http://www.dest-unreach.org/socat/download/socat-1.7.3.4.tar.gz`

`tar xzf socat-1.7.3.4.tar.gz`

`cd socat-1.7.3.4/`

`./configure`

`make`

`sudo su`

 `make install`
 
Y luego probar de nuevo
