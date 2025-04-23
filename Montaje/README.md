# Montaje de Tinycorder

## Esquema del dispositivo:

ATENCION: Algunos componentes no se muestran como son realmente ya que Fritzing no los contiene en las librerías. Y puede haber algun error en la nomenclatura de los pines. Tomar la información de este montaje con precaución.


![Esquemático](Tinycorder_schematic.jpg)

## Montaje del dispositivo:

En esta imagen vemos los componentes que vamos a utilizar. La pantalla usa el bus SPI y los sensores el I2C, de solo dos pines.

![Componentes](componentes.jpg)

Procedemos a soldar los componentes entre si.

En la perfboard soldamos el MCU, switch, 3 pushbutton y header de dos puntas. Para el XIAO alinear bien de tal forma que sus dos pad de bateria queden accesibles desde abajo, ya que aplicaremos una gota de soldadura y debe servir para que el circuito de control de la bateria funcione bien. Lo sabremos si al conectar el puerto USB tenemos unos 4V en esas dos conexiones. Si no lo tenemos hay que repasar la soldadura.

La perfboard la unimos al display con soldadura (tiene zonas metalicas donde el estaño agarra bien. Tambien puede hacerse con cola termofusible).


![Cableado](cableado.jpg)

La batería la pegaremos con cola termofusible, igualmente los sensores, aprovechando el espacio libre en la parte trasera del display que es plano.

**ATENCION: Diseñé las piezas en PLA después de este punto con las medidas tal y como coloqué los componentes. Si construyes una copia tendrás que hacerlo al revés. Imprimir las piezas y usarlas como guía para pegar os sensores, si no no te cabrán correctamente.**

![Montaje y soldadura](colocado_y_soldado.jpg)

Una vez montado y cargado el software vemos el aspecto que tiene.

![Prueba Badge](ok_badge.jpg)

Y aquí como espectrómetro numérico, los valores se ven bajos porque el sensor está contra la mesa y no recibe luz.

![Espectrómetro numérico](ok_espectrometro_numerico.jpg)

***

