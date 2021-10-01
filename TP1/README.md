# Protos - TPE 1
### Instrucciones de instalación y compilado:
- Descomprimir el .zip en el cual se tiene el proyecto
- Ingresar al directorio TP1
- Ejecutar el comando `make`
- Ejecutar el comando `./server` creado a partir del make
- Conectarse desde otra terminal al servidor (usando nc)

### Manual de uso:
##### Desde el servidor:
`./server [port]`
Si no se especifica ningún puerto, por defecto corre sobre el puerto 9999.
##### Conectándose al servidor:
- `nc -C localhost [port]` para conexiones TCP.
- `nc -u localhost [port]` para conexiones UDP.

### Comandos soportados:
##### Desde TCP:
- `GET date` imprime la fecha actual (en el formato en el cual esté trabajando el servidor, ver sección siguiente para más información)
- `GET time` devuelve la hora actual en formato hh:mm:ss
- `ECHO texto` devuelve el texto enviado
##### Desde UDP:
- `SET locale es` configura al servidor en modo español (la fecha se devuelve en dd/mm/yyyy)
- `SET locale en` configura al servidor en modo inglés (la fecha se devuelve en mm/dd/yyyy)
- `STATS` devuelve estadísticas sobre el servidor (cantidad de conexiones, líneas correctas, líneas incorrectas y datagramas incorrectos desde que se inició el servidor)