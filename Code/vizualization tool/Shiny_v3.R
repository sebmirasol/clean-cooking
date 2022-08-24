library(ggplot2)
library(shiny)
library(readr)
library(dplyr)
library(shinyWidgets)
library(shinythemes)
library(shinydashboard)
library(leaflet)
library(readxl)
library(fresh)
library(fontawesome)
library(lubridate)
library(anytime)
library(showtext)
library(sysfonts)
library(showtextdb)

# Carga de csv_________________________________________________________________
data <- read_csv("./data.csv", 
                 col_types = cols(X1 = col_skip()))

explicaciones <- read_excel("explicaciones.xlsx")

factores <- read_excel("variables_factores.xlsx")
factores2 <- factores %>% 
  mutate(breaks_2 = lapply(strsplit(breaks, ','), FUN = as.numeric),
         CAM = strsplit(labels_CAM,","),
         ENG = strsplit(labels_ENG,","),
         paleta = strsplit(paleta_colores,","))

if (file.exists("coords.csv")){
  coords <- read_csv("./coords.csv")
} else{
  coords <- data.frame(ID=c(1,2,3,4,5), Latitud = c("","","","",""), Longitud = c("","","","",""))
}
#______________________________________________________________________________

# Variables globales___________________________________________________________

## Umbral del CO2 para considerar que se queman residuos

umbral <- 700

## Número de datos que conforman una hora. Ahora, sampleo cada 15 mins:

n_barras <- 4


# Cargando fuentes de  Google (https://fonts.google.com/)
#font_add_google("Hanuman", "hanuman")

#______________________________________________________________________________

ui <- navbarPage(
  theme = shinytheme("journal"),
  #Para probar otros estilos
  #shinythemes::themeSelector(),
  

  
  
  title = div(div(id = "img-id",
      img(src = "logosNB.png",
          width= "280px", 
          height="40px")
    ),
    "Air Quality Visualization"
  ),
  
  #Estilos_____________________________________________________________________
  
  #Estilo InfoBoxes
  header = tagList(
    useShinydashboard()
  ),
  
  tags$head(tags$style(HTML('.info-box {min-height: 75px;} .info-box-icon {height: 75px; line-height: 75px;}
                            .info-box-content {padding-top: 0px; padding-bottom: 0px;}'))),
  
  #Estilo para el texto
  tags$head(tags$style(HTML("#explicacion{color: black;
                                 font-size: 25px;
                                 font-family: Arial;
                                 }"
    ))),
 
  tags$head(tags$style(HTML('* {font-family: "Arial"};'))),
  
  #Estilo para el logo
  tags$head(tags$style(
    type="text/css",
    "#img-id{
    position: fixed;
    display:right-align;
    right: 10px;
    top: 5px;
    }"
  )),
 
 # Estilo del cuadro de diálogo
  use_theme(
    create_theme(
      theme = "journal",
      bs_vars_modal(
        md = "40%",
        backdrop_opacity = 0.3,
        backdrop_bg = "#f4a020",
        header_border_color = "#f4a020",
        footer_border_color = "#f4a020"
      ),
      output_file = NULL
    )
  ),
#______________________________________________________________________________

#Panel Lateral_________________________________________________________________
  sidebarLayout(
    sidebarPanel(
      #Mapa de los aparatos
      leafletOutput("mapa_interactivo", width = "100%"),
      br(),
      #Seleccion de idioma
      fluidRow(column(2, dropdownButton(
        selectInput("idioma","",
                    c("ENG"), #, "CAM"
                    selected = "ENG"),
        circle = TRUE, status = "default",
        icon = icon("gear"), width = "300px"
      )),
      #Botones
      column(5, uiOutput("boton_coordenadas")
      ),
      column(5, uiOutput("boton_general")
             )),
      br(),
      uiOutput("InfoBoxes")
      , width = 4),#sidebarpanel
  #____________________________________________________________________________    
  
  # Panel Principal____________________________________________________________

      mainPanel(
        
      #Seleccion de variable
        
      uiOutput("boton_variable"),
      
      fluidRow(column(6, uiOutput("boton_id_resumen")),
               column(6,uiOutput("boton_fechas"))),
      
      
      #Grafico central
      plotOutput("main_plot"),
      br(),
      br(),
      #Información variables
      infoBox(title = "",textOutput("explicacion"), 
          width = 12, icon = icon("info-circle"), color = "navy"),
      br(),
      br(),
      #InfoBoxes - huella carbono y residuos
      uiOutput("InfoBoxes_adicionales")
      
    )
  )
)
#______________________________________________________________________________


server <- function(input, output, session) {
  
  #Grafico Central_____________________________________________________________
  output$main_plot <- renderPlot({
    
    #Validacion de inputs
    if (input$idioma == "CAM"){
      validate(
        need(input$id, "សូមអភ័យទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសឧបករណ៍ណាមួយទេ។
             សូមជ្រើសរើសយ៉ាងហោចណាស់មួយ។"
        )
      )
      validate(
        need(input$dates, "យើង·សុំទោស,ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
             សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
        )
      )
      
      validate(
        need(input$dates[2] > input$dates[1],  "យើង·សុំទោស,ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
             សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
        ))
      lab_x = "កាលបរិច្ឆេទ"
      
    } else if (input$idioma == "ENG"){
      validate(
        need(input$id, "Sorry, but you have not selected any device. 
                      Please select at least one."
        )
      )
      
      validate(
        need(input$dates, "Sorry, there is no dates for you requested combination. 
                      Please add an input selection"
        )
      )
      
      validate(
        need(input$dates[2] > input$dates[1], "Sorry, there is no dates for you requested combination. 
                      Please modify the input selection"
        )
      )
      lab_x = "Date"
    }
    
    
    #Grafico
    data %>%
      filter(
        Date >= as.POSIXct(input$dates[1]),
        Date <= as.POSIXct(input$dates[2]),
        ID %in% as.integer(input$id)
      ) %>%
      mutate(ID = as.factor(ID)) %>% 
      ggplot(aes_string(x = "Date", y = input$variable, group = "ID")) +
      geom_line(aes(col = ID)) +
      geom_point()+
      theme_minimal()+
      theme(legend.title = element_text(size=15, 
                                          face="bold"))+
      theme(legend.text = element_text(size=15, 
                                         face="bold")) +
      labs(
        x = lab_x,
        y = input$variable
      )
  })
  #____________________________________________________________________________
  
  #Boton -Volver al panel general-_____________________________________________
  observeEvent(input$clean,{
    
    output$boton_id_resumen <- renderUI({
      
      if (input$idioma == "CAM"){
          label = "ជ្រើសរើសឧបករណ៍ដែលអ្នកចង់មើល"
          choices = c("ឧបករណ៍ 1" = "1","ឧបករណ៍ 2" = "2","ឧបករណ៍ 3" = "3",
                      "ឧបករណ៍ 4" = "4","ឧបករណ៍ 5" = "5")
          } else if (input$idioma == "ENG"){
            label = "Select the device you want to display"
            choices = c("Aparato 1" = "1","Aparato 2" = "2","Aparato 3" = "3",
                        "Aparato 4" = "4","Aparato 5" = "5")
          }
      
      checkboxGroupButtons(
        inputId = "id",
        label = label,
        choices = choices,
        selected = "1")
      
    })
    
    
    #Eliminar boton -volver al panel general- en el panel general
    
    output$boton_general <- renderUI({  })
  
    
    #Volver a generar el plot principal
    
    output$main_plot <- renderPlot({
      
      if (input$idioma == "CAM"){
        validate(
          need(input$id, "សូមអភ័យទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសឧបករណ៍ណាមួយទេ។
             សូមជ្រើសរើសយ៉ាងហោចណាស់មួយ។"
          )
        )
        
        validate(
          need(input$dates, "យើង·សុំទោស,ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
             សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
          )
        )
        
        validate(
          need(input$dates[2] > input$dates[1],  "យើង·សុំទោស,ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
             សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
          ))
        
        lab_x = "កាលបរិច្ឆេទ"
        
      } else if (input$idioma == "ENG"){
        validate(
          need(input$id, "Sorry, but you have not selected any device. 
                      Please select at least one."
          )
        )
        
        validate(
          need(input$dates, "Sorry, but you have not selected any dates 
                      Please select two dates."
          ))
        
        validate(
          need(input$dates[2] > input$dates[1], "Sorry, there is no dates for you requested combination. 
                      Please modify the input selection"
          ))
        
        lab_x = "Date"
      }
      
      
      
      data %>%
        filter(
          Date >= as.POSIXct(input$dates[1]),
          Date <= as.POSIXct(input$dates[2]),
          ID %in% as.integer(input$id)
        ) %>%
        mutate(ID = as.factor(ID)) %>% 
        ggplot(aes_string(x = "Date", y = input$variable, group = "ID")) +
        geom_line(aes(col = ID)) +
        geom_point()+
        theme_minimal()+
        theme(legend.title = element_text(size=15, 
                                          face="bold"))+
        theme(legend.text = element_text(size=15, 
                                         face="bold")) +
        labs(
          x =lab_x,
          y = input$variable
        )
    })
    
    #Volver a generar las InfoBox
    
    output$InfoBoxes <- renderUI({
      
      if(input$idioma == "CAM"){
        titulos = c("សីតុណ្ហភាព", "សំណើម", "ស្គរ")
      } else if(input$idioma == "ENG"){
        titulos = c("Temperature", "Humidity", "Battery")
      }
        fluidPage(fluidRow(column(6,
                                  infoBox(titulos[1], "-", icon = icon("thermometer-half",lib = "font-awesome"),color = "red", width = NULL, fill = TRUE)),
                           column(6,infoBox(titulos[2], "-", icon = icon("tint", lib = "font-awesome"),color = "light-blue", width = NULL, fill = TRUE))),#FluidRow
                  
                  fluidRow(column(6,
                                  infoBox("CO", "-", icon = icon("lungs", lib = "font-awesome"),color = "teal", width = NULL, fill = TRUE)),
                           column(6,infoBox("CO2", "-", icon = icon("lungs", lib = "font-awesome"),color = "teal", width = NULL, fill = TRUE))),
                  
                  fluidRow(column(6,
                                  infoBox("PM 1", "-", icon = icon("viruses", lib = "font-awesome"), color = "maroon",width = NULL, fill = TRUE)),
                           column(6,infoBox("PM 2.5", "-", icon = icon("viruses", lib = "font-awesome"), color = "maroon",width = NULL, fill = TRUE))),
                  fluidRow(column(6,
                                  infoBox("PM 10", "-", icon = icon("viruses", lib = "font-awesome"), color = "maroon", width = NULL, fill = TRUE)),
                           column(6,infoBox(titulos[3], "-", icon = icon("battery-3", lib = "font-awesome"),color = "orange", width = NULL, fill = TRUE))))
      
    })
    
    #Eliminar infoboxes con huella de carbono y residuos
    
    output$InfoBoxes_adicionales <- renderUI({ })
    
    
  })
  #____________________________________________________________________________
  
  
  #Clicar sobre el mapa________________________________________________________
 
  
   observeEvent(input$mapa_interactivo_marker_click, { 
   
     
     click <- input$mapa_interactivo_marker_click
     
    #Botón para volver al panel comparativo 
    output$boton_general <- renderUI({
      
      if (input$idioma == "CAM"){
          label = "ទិដ្ឋភាពទូទៅនៃផ្ទាំងគ្រប់គ្រង"
      } else if (input$idioma == "ENG"){
            label = "Compare Devices"
            }
      actionBttn(
        inputId = "clean",
        label = label,
        style = "pill", 
        color = "primary"
      )
      
     
    })
    
    #Boton resumen para agrupar datos
    output$boton_id_resumen <- renderUI({
      
      if (input$idioma == "CAM"){
          label = "ជ្រើសរើសរយៈពេលដែលអ្នកចង់ដាក់ជាក្រុមទិន្នន័យ៖"
          choices = c("ម៉ោង" = "h", "ប្រចាំថ្ងៃ" = "d", "ប្រចាំសប្តាហ៍" = "s", "ប្រចាំឆ្នាំ" = "a")
        } else if (input$idioma == "ENG"){
            label = "Select the time span you want to group the data by:"
            choices = c("Hour" = "h", "Diary" = "d", "Weekly" = "s", "Yearly" = "a")
        }
      
      pickerInput(
        inputId = "Resumen",
        label = label, 
        choices = choices,
        selected = "d"
      )
      
    })
    
    #Nuevo gráfico central
        
    output$main_plot <- renderPlot({
      
      if (input$idioma == "CAM"){
        validate(
          need(input$dates, "យើងសុំទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
               សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
          ))
        
        validate(
          need(input$dates[2] > input$dates[1],  "យើងសុំទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
               សូមជ្រើសរើសកាលបរិច្ឆេទពីរ។"
          ))
        
      } else if (input$idioma == "ENG"){
        validate(
          need(input$dates, "Sorry, there is no dates for you requested combination. 
                      Please add an input selection"
          ))
        
        validate(
          need(input$dates[2] > input$dates[1], "Sorry, there is no dates for you requested combination. 
                      Please modify the input selection"
          ))
      }
      
      
      if(input$Resumen == "h"){
        time <- "1 hour"
      } else if(input$Resumen == "d"){
        time <- "1 day"
      }else if(input$Resumen == "s"){
        time <- "1 week"
      } else if(input$Resumen == "a"){
        time <- "1 year"
        }
      
      
      
      data %>%
        filter(
          Date >= as.POSIXct(input$dates[1]),
          Date <= as.POSIXct(input$dates[2]),
          ID == click$id) %>%
          select(input$variable,Date)%>%
          rename("variable" = input$variable,
                 "Date" = "Date")%>%
          group_by(Date=floor_date(Date, time)) %>%
          summarize(variable = mean(variable))%>%
        mutate(Clasificacion = cut(variable,
                                   breaks = factores2 %>% filter(variables == input$variable) %>% select(breaks_2) %>% pull() %>% unlist(),
                                   labels = factores2 %>% filter(variables == input$variable) %>% select(input$idioma) %>% pull() %>% unlist())) %>%
        ggplot(aes(x=Date, y=variable)) +
        geom_bar(stat="identity", aes(fill=Clasificacion)) +
        geom_text(aes(label=round(variable,2)), size = 5, vjust=-0.2)+
        scale_fill_manual(values = factores2 %>% filter(variables == input$variable) %>% select(paleta) %>% pull() %>% unlist(),
                          drop = FALSE) + 
        
        theme_minimal() +
        guides(fill=guide_legend(title=NULL)) +
        theme(legend.text = element_text(size=15, face="bold"))
        })
        
     
    
    #InfoBoxes
      values <- data %>%
        filter(
          Date >= as.POSIXct(input$dates[1]),
          Date <= as.POSIXct(input$dates[2]),
          ID %in% as.integer(input$id)
        ) %>%
        summarize(min_Tem = min(Tem), max_Tem = max(Tem),
                  min_Hum = min(Hum), max_Hum = max(Hum),
                  min_CO2 = min(CO2), max_CO2 = max(CO2),
                  min_CO = min(CO), max_CO = max(CO),
                  min_PM1 = min(PM1), max_PM1 = max(PM1),
                  min_PM2 = min(PM2), max_PM2 = max(PM2),
                  min_PM10 = min(PM10), max_PM10 = max(PM10),
                  min_Bat = min(Bat), max_Bat = max(Bat)) %>%
        round(2)
      
      value_Tem <- HTML(paste("Max ", as.character(values$max_Tem), "ºC",
                              br(),  "Min ",as.character(values$min_Tem), "ºC"))
      
      value_Hum <- HTML(paste("Max ", as.character(values$max_Hum), "%",
                              br(),  "Min ",as.character(values$min_Hum), "%"))
      
      value_CO2 <- HTML(paste("Max ", as.character(values$max_CO2), "ppm",
                              br(),  "Min ",as.character(values$min_CO2), "ppm"))
      
      value_CO <- HTML(paste("Max ", as.character(values$max_CO), "ppm",
                              br(),  "Min ",as.character(values$min_CO), "ppm"))
      
      value_PM1 <- HTML(paste("Max ", as.character(values$max_PM1), "ppm",
                              br(),  "Min ",as.character(values$min_PM1), "ppm"))
      
      value_PM2 <- HTML(paste("Max ", as.character(values$max_PM2), "ppm",
                              br(),  "Min ",as.character(values$min_PM2), "ppm"))
      
      value_PM10 <- HTML(paste("Max ", as.character(values$max_PM10), "ppm",
                              br(),  "Min ",as.character(values$min_PM10), "ppm"))
      
      value_Bat <- HTML(paste("Max ", as.character(round(values$max_Bat),0), "%",
                              br(),  "Min ",as.character(round(values$min_Bat),0), "%"))
      
      
      output$temperatura<- renderInfoBox({
        
        
        if (input$idioma == "CAM"){
          titulo = c("សីតុណ្ហភាព")
        } else if (input$idioma == "ENG"){
          titulo = c("Temperature")
        }
        
        infoBox(
          titulo, value_Tem, 
          icon = icon("thermometer-half",lib = "font-awesome"),
          color = "navy", fill = TRUE
        )
      })
      
      
      output$humedad <- renderInfoBox({
        
        if (input$idioma == "CAM"){
          titulo = c("សំណើម")
        } else if (input$idioma == "ENG"){
          titulo = c("Humidity")
        }
        infoBox(
          titulo, value_Hum, 
          icon = icon("tint", lib = "font-awesome"),
          color = "navy", fill = TRUE
        )
        
      })
      
      output$CO2<- renderInfoBox({
        infoBox(
          "CO2", value_CO2, icon = icon("lungs", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      output$CO<- renderInfoBox({
        infoBox(
          "CO", value_CO, icon = icon("lungs", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      output$PM1<- renderInfoBox({
        infoBox(
          "PM1", value_PM1, icon = icon("viruses", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      output$PM2<- renderInfoBox({
        infoBox(
          "PM2", value_PM2, icon = icon("viruses", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      output$PM10<- renderInfoBox({
        infoBox(
          "PM10", value_PM10, icon = icon("viruses", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      
      output$Batery<- renderInfoBox({
        
        if (input$idioma == "CAM"){
          titulo = c("ស្គរ")
        } else if (input$idioma == "ENG"){
          titulo = c("Battery")
        }
        
        infoBox(
          titulo, value_Bat, icon = icon("battery-3", lib = "font-awesome"),
          color = "navy", fill = TRUE)
      })
      
      
    #Render InfoBoxes
      
      output$InfoBoxes <- renderUI({
        
        fluidPage( fluidRow(column(6,
                        infoBoxOutput("temperatura", width = NULL)),
        column(6,infoBoxOutput("humedad", width = NULL))),#FluidRow
        
        fluidRow(column(6,
                        infoBoxOutput("CO", width = NULL)),
        column(6,infoBoxOutput("CO2", width = NULL))),
        
        fluidRow(column(6,
                        infoBoxOutput("PM1", width = NULL)),
        column(6,infoBoxOutput("PM2", width = NULL))),
        fluidRow(column(6,
                        infoBoxOutput("PM10", width = NULL)),
        column(6,infoBoxOutput("Batery", width = NULL))))
        
      })
      
      #InfoBoxes - Huella de carbono y recursos
      
      output$InfoBoxes_adicionales <- renderUI({
        
        if (input$idioma == "CAM"){
          validate(
            need(input$dates, "សូមអភ័យទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
                      សូមជ្រើសរើសយ៉ាងហោចណាស់មួយ។"
            ))
          validate(
            need(input$dates[2] > input$dates[1],  "សូមអភ័យទោស ប៉ុន្តែអ្នកមិនបានជ្រើសរើសកាលបរិច្ឆេទណាមួយទេ។
                      សូមជ្រើសរើសយ៉ាងហោចណាស់មួយ។"
            ))
        } else if (input$idioma == "ENG"){
          validate(
            need(input$dates, "Sorry, but you have not selected any dates 
                      Please select at least one."
            ))
          validate(
            need(input$dates[2] > input$dates[1], "Sorry, there is no dates for you requested combination. 
                      Please modify the input selection"
            ))
          
          
        }
        
        
        aux <- data %>%
          filter(
            Date >= as.POSIXct(input$dates[1]),
            Date <= as.POSIXct(input$dates[2]),
            ID %in% as.integer(input$id)) %>%
          select(CO2)%>%
          filter(CO2 >= umbral) %>%
          summarise(n = (n()/n_barras))
        
        days <- data %>%
          filter(
            Date >= as.POSIXct(input$dates[1]),
            Date <= as.POSIXct(input$dates[2]),
            ID %in% as.integer(input$id)) %>%
          group_by(Date=floor_date(Date, "1 day")) %>%
          summarise(Date = unique(Date)) %>%
          summarise(n = n())
        
        if(input$idioma == "CAM"){
          titulos = c("ដានកាបូន", "មធ្យោបាយ")
          valor_huella = HTML(paste("តម្លៃរបស់អ្នក៖ ",aux*0.0032, "តោន CO2", br(), "បានណែនាំ: ", days*0.0046, "តោន CO2"))
          valor_recursos = HTML("ធនធានត្រូវបានដុតបំផ្លាញ៖ ",paste(aux*2, "Kg"))
        } else if(input$idioma == "ENG"){
          titulos = c("Carbon footprint in selected period", "Resources consumed in selected period")
          valor_huella = HTML(paste("Etimated: ",aux*0.0032, "tons CO2", br(),"Mean: ", days*0.0046, "tons CO2" ))
          valor_recursos = HTML(paste("Estimated wood/coal burned: ",aux*2, "Kg"))
        }
        
        
        
        fluidPage(fluidRow(column(6,
                                  infoBox(titulos[1], valor_huella, icon = icon("thermometer-half",lib = "font-awesome"),
                                          color = "olive", width = NULL, fill = TRUE)),
                           column(6,infoBox(titulos[2], valor_recursos, icon = icon("fire", lib = "font-awesome"),
                                            color = "olive", width = NULL, fill = TRUE))))
      })
      
  })
  
  #____________________________________________________________________________
  
  # Boton id___________________________________________________________________
  
  output$boton_id_resumen <- renderUI({
    
    if (input$idioma == "CAM"){
        label = "ជ្រើសរើសឧបករណ៍ដែលអ្នកចង់មើល"
        choices = c("ឧបករណ៍ 1" = "1","ឧបករណ៍ 2" = "2","ឧបករណ៍ 3" = "3",
                    "ឧបករណ៍ 4" = "4","ឧបករណ៍ 5" = "5")
      } else if (input$idioma == "ENG"){
          label = "Select the device you want to display"
          choices = c("Device 1" = "1","Device 2" = "2","Device 3" = "3",
                      "Device 4" = "4","Device 5" = "5")
          }
    checkboxGroupButtons(
      inputId = "id",
      label = label,
      choices = choices,
      selected = "1"
    )
    
  })
  #____________________________________________________________________________
  
  # Boton variable - Idioma____________________________________________________
  
  output$boton_variable <- renderUI({
    
    if (input$idioma == "CAM"){
      choices = c("សីតុណ្ហភាព" = "Tem","សំណើម" = "Hum", "CO2", "CO",
                  "PM 1.0" = "PM1", "PM 2.5" = "PM2", "PM 10"="PM10", "Batería" = "Bat")
    } else if (input$idioma == "ENG"){
      choices = c("Temperature" = "Tem","Humidity" = "Hum", "CO2", "CO",
                  "PM 1.0" = "PM1", "PM 2.5" = "PM2", "PM 10"="PM10", "Battery" = "Bat")
    }
    
    radioGroupButtons(
      inputId = "variable",
      label = "",
      choices = choices,
      selected = "Tem",
      justified = TRUE
    )
  })
  #____________________________________________________________________________
  
  # Boton fechas - Idioma_____________________________________________________
  
  output$boton_fechas <- renderUI({
    
    
    if (input$idioma == "CAM"){
      label = "ជ្រើសរើសកាលបរិច្ឆេទពីរ"
    } else if(input$idioma == "ENG"){
      label = "Select two dates:"
    }
    
    options <- timepickerOptions(
      dateTimeSeparator = " ",
      timeFormat = "h:i",
      minHours = 0,
      maxHours = 23,
      minMinutes = 0,
      maxMinutes = 59,
      hoursStep = 1,
      minutesStep = 15
    )
    
    airDatepickerInput(
      inputId = "dates",
      label = label,
      value = c("2018-07-01 12:00","2018-07-4 12:00"),
      multiple = 2,
      dateFormat = "yyyy-mm-dd",
      timepicker = TRUE,
      view = c("days", "months", "years"),
      minView = c("days", "months", "years"),
      monthsField = c("monthsShort", "months"),
      clearButton = TRUE,
      timepickerOpts = options,
      update_on = c("change", "close"),
      addon = c("right", "left", "none"),
      language = "en",
      toggleSelected = TRUE
    )
    
  })
  #____________________________________________________________________________
  
  # Boton coordenadas - Idioma_____________________________________________________
  
  output$boton_coordenadas <- renderUI({
    
    if (input$idioma == "CAM"){
        label = "កូអរដោនេ"
      } else if (input$idioma == "ENG"){
        label = "Coordinates"
  }
    
    actionBttn(
      inputId = "cambio_mapa",
      label = label,
      style = "pill", 
      color = "warning")
  })
  #____________________________________________________________________________
  
  
  #InfoBoxes___________________________________________________________________
  
  output$InfoBoxes <- renderUI({
    
    if(input$idioma == "CAM"){
      titulos = c("Temperatura", "Humedad", "Batería")
    } else if(input$idioma == "ENG"){
      titulos = c("Temperature", "Humidity", "Battery")
    }
    
    fluidPage(fluidRow(column(6,
                               infoBox(titulos[1], "-", icon = icon("thermometer-half",lib = "font-awesome"),color = "navy", width = NULL, fill = TRUE)),
                        column(6,infoBox(titulos[2], "-", icon = icon("navy", lib = "font-awesome"),color = "navy", width = NULL, fill = TRUE))),#FluidRow
               
               fluidRow(column(6,
                               infoBox("CO", "-", icon = icon("lungs", lib = "font-awesome"),color = "navy", width = NULL, fill = TRUE)),
                        column(6,infoBox("CO2", "-", icon = icon("lungs", lib = "font-awesome"),color = "navy", width = NULL, fill = TRUE))),
               
               fluidRow(column(6,
                               infoBox("PM 1", "-", icon = icon("viruses", lib = "font-awesome"), color = "navy",width = NULL, fill = TRUE)),
                        column(6,infoBox("PM 2.5", "-", icon = icon("viruses", lib = "font-awesome"), color = "navy",width = NULL, fill = TRUE))),
               fluidRow(column(6,
                               infoBox("PM 10", "-", icon = icon("viruses", lib = "font-awesome"), color = "navy", width = NULL, fill = TRUE)),
                        column(6,infoBox(titulos[3], "-", icon = icon("battery-3", lib = "font-awesome"),color = "navy", width = NULL, fill = TRUE))))
    
  })
  
  
 #_____________________________________________________________________________
  

  #Mapa interactivo____________________________________________________________
  observeEvent(input$ok,{
    
    if (file.exists("coords.csv")){
      coords <<- read_csv("./coords.csv")
    } else{
      coords <<- data.frame(ID=c(1,2,3,4,5), Latitud = c("","","","",""), 
                            Longitud = c("","","","",""))
    }
    
    #Actualizar Longitudes
    coords$Longitud[1] <- input$longitud1
    coords$Longitud[2] <- input$longitud2
    coords$Longitud[3] <- input$longitud3
    coords$Longitud[4] <- input$longitud4
    coords$Longitud[5] <- input$longitud5
    
    #Actualizar Latitudes
    coords$Latitud[1] <- input$latitud1
    coords$Latitud[2] <- input$latitud2
    coords$Latitud[3] <- input$latitud3
    coords$Latitud[4] <- input$latitud4
    coords$Latitud[5] <- input$latitud5
    
    #Actualizar Coordenadas
    write.csv(coords,"./coords.csv", row.names = FALSE)
    
    output$mapa_interactivo <- renderLeaflet({
      
      if (input$idioma == "CAM"){
        label = paste0("ឧបករណ៍ ", coords$ID)
        } else if (input$idioma == "ENG"){
          label = paste0("Device ", coords$ID)
          }
      
      leaflet() %>%
        addProviderTiles("CartoDB") %>%
        addCircleMarkers(
          lng = as.numeric(coords$Longitud),
          lat = as.numeric(coords$Latitud),
          label = label,
          layerId = coords$ID,
          radius = 8,
          color = "#e51d2e")
    })
  })
  #____________________________________________________________________________
  
  #Output mapa_________________________________________________________________
  
  output$mapa_interactivo <- renderLeaflet({
    
    if (input$idioma == "CAM"){
      label = paste0("ឧបករណ៍ ", coords$ID)
    } else if (input$idioma == "ENG"){
      label = paste0("Device ", coords$ID)
    }
    
    
    leaflet() %>%
      addProviderTiles("CartoDB") %>%
      addCircleMarkers(
        lng = as.numeric(coords$Longitud),
        lat = as.numeric(coords$Latitud),
        label = label,
        layerId = coords$ID,
        radius = 8,
        color = "#e51d2e")
  })
  
  #____________________________________________________________________________
  
  #Texto explicativo___________________________________________________________
  
  observeEvent(input$variable,{
    
    output$explicacion <- renderText({
      
      explicaciones%>%
        filter(Idioma==input$idioma)%>%
        select(input$variable) %>% 
        pull()

    })
  })
  
  #____________________________________________________________________________   
  
  
    #Apartado de modifiacion de coordenadas____________________________________
    # Crear objeto para guardar los valores reacctivos
    vals <- reactiveValues(
      txt = NULL,
      error_msg = NULL,
      print = FALSE
    )
    
    # Crear modal
    popupModal <- function() {
      
      if(input$idioma == "CAM"){
        titulos = c("កែប្រែ", "ឧបករណ៍ 1", 
                    "ឧបករណ៍ 2", "ឧបករណ៍ 3", "ឧបករណ៍ 4", "ឧបករណ៍ 5", 
                    "ជ្រើសរើសប្រវែង", "ជ្រើសរើសរយៈទទឹង", "បោះបង់")
      }else if(input$idioma == "ENG"){
        titulos = c("Modify the device coordinates", "Device 1", 
                    "Device 2", "Device 3", "Device 4", "Device 5", 
                    "Select longitude", "Select latitude", "Cancel")
      }
        
      modalDialog(
        title = tags$h3(titulos[1]),
        fluidRow(column(12), tags$h4(titulos[2])),
        fluidRow( column(6,
                         #Latitud
                         textInput(
                           inputId = "latitud1",
                           label = titulos[8],
                           value = coords$Latitud[1])
                         ),
                  
                  #Longitud
                  column(6,
                         textInput(
                           inputId = "longitud1",
                           label = titulos[7],
                           value = coords$Longitud[1])
                         )
                  ),
        fluidRow(column(12), tags$h4(titulos[3])),
        fluidRow(column(6,
                         #Latitud
                         textInput(
                           inputId = "latitud2",
                           label = titulos[8],
                           value = coords$Latitud[2])
                         ),
                  #Longitud
                  column(6,
                         textInput(
                           inputId = "longitud2",
                           label = titulos[7],
                           value = coords$Longitud[2])
                         )
                  ),
        fluidRow(column(12), tags$h4(titulos[4])),
        fluidRow(column(6,
                         #Latitud
                         textInput(
                           inputId = "latitud3",
                           label = titulos[8],
                           value = coords$Latitud[3]
                         )),
                  
                  #Longitud
                  column(6,
                         textInput(
                           inputId = "longitud3",
                           label = titulos[7],
                           value = coords$Longitud[3])
                         )
                  ),
        fluidRow(column(12), tags$h4(titulos[5])),
        fluidRow(column(6,
                         #Latitud
                         textInput(
                           inputId = "latitud4",
                           label = titulos[8],
                            value = coords$Latitud[4]
                         )),
                  
                  #Longitud
                  column(6,
                         textInput(
                           inputId = "longitud4",
                           label = titulos[7],
                           value = coords$Longitud[4]
                         ))
                  ),
        fluidRow(column(12), tags$h4(titulos[6])),
        fluidRow( column(6,
                         #Latitud
                         textInput(
                           inputId = "latitud5",
                           label = titulos[8],
                           value = coords$Latitud[5]
                         )),
                  
                  #Longitud
                  column(6,
                         textInput(
                           inputId = "longitud5",
                           label = titulos[7],
                           value = coords$Longitud[5]
                         )
                  )),
        
        footer = tagList(
          actionBttn("cancel",titulos[9],style = "pill",
                     color = "danger"),
          actionBttn("ok", "OK",
                       style = "pill",
                       color = "success")
        )
      )
    }
#______________________________________________________________________________
    
 # Cuadro de diálogo___________________________________________________________
    
    observeEvent(input$cambio_mapa, {
      if (file.exists("coords.csv")){
        coords <<- read_csv("./coords.csv")
      } else{
        coords <<- data.frame(ID=c(1,2,3,4,5), Latitud = c("","","","",""), Longitud = c("","","","",""))
      }
      
      vals$error_msg <- NULL
      showModal(popupModal())
    })
    
    #Cerrar el cuadro de dialogo cuando se pulsa OK
    observeEvent(input$ok,{
        removeModal()})
    #Cerrar el cuadro de dialogo cuando se pulsa CANCELAR
    observeEvent(input$cancel,{
      removeModal()})
}
#______________________________________________________________________________
shinyApp(ui = ui, server = server)