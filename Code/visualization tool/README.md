# Proyecto_Wurth

## Español:

Archivos del proyecto:

- Shiny_v3.R: Versión final de la app de shiny, ejecutable.
- Hanuman-Regular.ttf: Tipografía que debería soportar los carácteres para el idioma jemer. La fuente Arial también debería funcionar.
- coords.csv: Contiene las coordenadas (inventadas) de los aparatos. Se puede borrar sin problema porque la propia app se encarga de crearlo en caso de no existir.
- explicaciones.csv: Contiene la explicaciones de las variables en 3 idiomas (español, inglés y camboyano), aunque solo se deberían utilizar las dos últimas.
- variables_factores.csv: Contiene los valores de corte (mortal, peligroso, etc..) para cada variable y su paleta de colores.
- data.csv: Contiene la simulación de los datos.


Consideraciones con el idioma:

- Todos los warnings que aparecen al ejecutar la app se producen por el cambio de idioma. La app funciona igual.
- Toda la app ha sido modificada para soportar ambos idiomas, pero hay problemas para renderizar los carácteres camboyanos en los botones.
Además, al consultar el texto en los csv (explicaciones y variables_factores) no coge el texto debidamente y devuelve un error.
- Desde la app no se puede modificar el idioma, así que ahora mismo solo está operativa en inglés.
- Cuando los problemas anteriores se resuelvan, bastará con añadir al selectInput de la línea 122 el valor comentado "CAM" 
para que vuelva a existir la selección de idiomas.



## English:

Project files:

- Shiny_v3.R: Final version of the shiny app, executable.
- Hanuman-Regular.ttf: Typeface that should support the characters for the Khmer language. Arial font should also work.
- coords.csv: Contains the (invented) coordinates of the devices. It can be deleted without problem because the app itself takes care of creating it in case it does not exist.
- explicaciones.csv: Contains the explanations of the variables in 3 languages (Spanish, English and Cambodian), although only the last two should be used.
- variables_factores.csv: Contains the cut-off values (lethal, dangerous, etc...) for each variable and its color palette.
- data.csv: Contains the simulation data.


Language considerations:

- All warnings that appear when running the app are caused by the language change. The app works the same.
- The whole app has been modified to support both languages, but there are problems rendering the Cambodian characters in the buttons.
Also, when querying the text in the csv (explanations and variables_factors) it does not catch the text properly and returns an error.
- From the app it is not possible to change the language, so right now it is only operative in English.
- When the previous problems are solved, it will be enough to add to the selectInput of the line 122 the commented value "CAM" 
to make the language selection available again.

