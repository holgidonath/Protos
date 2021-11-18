#### Seteo inicial
Para setear todo correctamente primero hacer: 
`sudo apt purge postfix dovecot-common dovecot-imapd dovecot-pop3d`

Estas instrucciones se realizarán en el usuario root: 
`sudo su`

 #### Postfix

Instalamos postfix: 
`apt-get install postfix`

En el asistente elegimos 
`"Sitio de Internet"`

Ponemos el nombre que querramos (para estandarizar y facilitar monitoreo diría de poner 'protos.tpe')

Una vez instalado vamos a `/etc/postfix/main.cf` y vemos que los parámetros son los que aclaramos cuando instalamos (en caso de que no aparezca myorigin, agregar la linea `myorigin= /etc/mailname`)

Comprobar si existe `/etc/mailname`. En caso de que no, crearlo y poner el nombre de dominio que pusimos arriba (protos.tpe)

#### Dovecot

Instalamos dovecot: 
`apt-get install dovecot-common dovecot-imapd dovecot-pop3d`

Una vez que está vamos a `/etc/dovecot/dovecot.conf`, nos fijamos que haya una linea que diga algo como `!include try...` y fijarse que admita todo los protocolos (`/usr/share/dovecot/protocols.d/*.protocol`)

Vamos a `/etc/dovecot/conf.d/10-auth.conf`, y en una sección que diga Authentication Process fijarse que esté descomentada la línea de disable_plaintext_auth y que sea igual a no; descomentamos también la línea que diga `!include auth-system.conf.ext`

En el archivo `/etc/dovecot/conf.d/20-pop3.conf` dejar descomentada la línea `pop3_uidl_format = %08Xu%08Xv`

En el archivo `/etc/dovecot/conf.d/10-mail.conf` dejar solamente descomentada la línea `mail_location = mbox:~/mail:INBOX=/var/mail/%u`

En el archivo `/etc/dovecot/conf.d/auth-system.conf.ext`, dentro de passdb asegurarse que la línea de args sea `args = session=yes failure_show_msg=yes dovecot`

#### Añadir un usuario y mandar mail para probar

`adduser userprotos` (este es un ejemplo)

Ahí pide datos, en la parte de contraseña recordarla porque es la que se usa en dovecot

Ahora hacemos un `service postfix restart` (si en algún momento no funciona colocar `systemctl reload postfix`)

Ahora lo que resta es mandar un mail (con postfix, `nc localhost 25` y así, asegurarse que se mande al usuario y dominio que eligieron, en este caso userprotos@protos.tpe)

Luego para comprobar podemos conectarnos al servidor (`nc localhost 110`)

Y ahí nos autenticamos 
`USER userprotos `
`PASS laquesea`
y pedimos los mails 
`LIST`
y debería aparecer el que acabamos de enviar.
