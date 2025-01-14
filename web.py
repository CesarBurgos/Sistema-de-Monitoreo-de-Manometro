from flask import Flask, render_template, Response
from flask import request as rq
from pydrive2.auth import GoogleAuth
from pydrive2.drive import GoogleDrive
from googleapiclient.discovery import build
from pydrive2.files import FileNotUploadedError

from google.auth.transport.requests import AuthorizedSession


import cv2
import urllib.request  # para abrir y leer URL
import numpy as np
from matplotlib.path import Path
import math
import base64
import imutils

import pandas as pd
import requests
import io
import tempfile
#pip install openpyxl
#pip install XlsxWriter xxx

directorio_credenciales = 'credentials_module.json'

app = Flask("Control_manometro")

# Areas de los objetos
V = [75, 35, 355, 245, 57641]
A = [40, 160, 220, 180, 19566]
R = [35, 270, 190, 95, 8463]
areas = [V, A, R]
secciones = []
bordes = []
datos_lectura = [None,None,None,None]

id_folder = '1Kt5QdVb7T5S7nNpD-5FL3H7AFmipS1aK'
id_drive = '1y1tqEFIVYFLqxyYPoRe-FWRXxvn-vCGn'
hoja_id = '1c_nq_BBT5_0z-7JSlQrUYDXJaEek-kW6nScDBh178y8'
sheet = 'datos_manometro.xlsx'

url = ''
valor = 0
img_processed = None

# ____ API GOOGLE DRIVE ___
# INICIAR SESION
def login():
    GoogleAuth.DEFAULT_SETTINGS['client_config_file'] = directorio_credenciales
    gauth = GoogleAuth()
    gauth.LoadCredentialsFile(directorio_credenciales)

    if gauth.credentials is None:
        gauth.LocalWebserverAuth(port_numbers=[8092])
    elif gauth.access_token_expired:
        gauth.Refresh()
    else:
        gauth.Authorize()

    gauth.SaveCredentialsFile(directorio_credenciales)
    credenciales = GoogleDrive(gauth)
    return gauth, credenciales

# BUSCAR LA ÚLTIMA IMAGEN CARGADA
def buscar_img_actual(id_folder):
    resultado = None
    _, credenciales = login()
    lista_archivos = credenciales.ListFile({'q': "'{}' in parents and trashed=false and mimeType = 'image/jpeg'".format(id_folder),
                                            'orderBy': 'modifiedDate desc'}).GetList()
    if lista_archivos:
        for f in lista_archivos:
            #datos_lectura.append(f['title'])
            datos_lectura[0] = f['id']
            datos_lectura[1] = f['createdDate']
            datos_lectura[3] = f['webContentLink']
            resultado = f['webContentLink']
            break
    return resultado


# ESCRITURA EN SHEET
def registro_inf2(id_folder):
    resultado = None
    gauth, credenciales = login()
    lista_archivos = credenciales.ListFile({'q': "'{}' in parents and trashed=false and mimeType = 'application/vnd.google-apps.spreadsheet'".format(id_folder),
                                            'orderBy': 'modifiedDate desc'}).GetList()
    if lista_archivos:
        #hoja_id = "1dFC94mM_taMeRhshEHBgdMRCqyxnbFa5"
        hoja_id = lista_archivos[0]['id']
        
        #url = "https://www.googleapis.com/drive/v2/files/" + hoja_id + "?alt=media" #"https://docs.google.com/spreadsheets/export?id={hoja_id}&exportFormat=xlsx"
        #1dFC94mM_taMeRhshEHBgdMRCqyxnbFa5
        url = "https://docs.google.com/spreadsheets/export?id=" + hoja_id + "&exportFormat=xlsx"
        #url = "https://docs.google.com/spreadsheets/export?id={hoja_id}&exportFormat=zip"
        res = requests.get(url, headers={"Authorization": "Bearer " + gauth.attr['credentials'].access_token,
                                         'Content-Type': 'application/vnd.google-apps.spreadsheet'})

        bio = io.BytesIO(res.content)
        values = pd.read_excel(bio, engine='openpyxl', header = 0)

        # Crear un nuevo DataFrame con las filas que desea agregar
        nuevas_filas = pd.DataFrame({
                'id_imagen': [datos_lectura[0]],
                'fecha': [datos_lectura[1]],
                'valor': [datos_lectura[2]],
                'url_imagen': [datos_lectura[3]]})

        # Concatenar el nuevo DataFrame con el DataFrame original
        values = pd.concat([values, nuevas_filas], ignore_index=True)

        with tempfile.NamedTemporaryFile(suffix='.xlsx', delete=False) as tmp:
            values.to_excel(tmp.name, index=False)
            tmp.flush()

            file1 = credenciales.CreateFile({'id': hoja_id})
            file1.Upload()
            file1.SetContentFile(tmp.name)
            file1.Upload()

# LECTURA EN SHEET
def registro_inf(id_folder):
    resultado = None
    gauth, credenciales = login()
    lista_archivos = credenciales.ListFile({'q': "'{}' in parents and trashed=false and mimeType = 'application/vnd.google-apps.spreadsheet'".format(id_folder),
                                            'orderBy': 'modifiedDate desc'}).GetList()
    if lista_archivos:
        #print(lista_archivos[0]['id'])
        #print(lista_archivos[0]['title'])

        #hoja_id = "1dFC94mM_taMeRhshEHBgdMRCqyxnbFa5"
        hoja_id = lista_archivos[0]['id']
        
        #url = "https://www.googleapis.com/drive/v2/files/" + hoja_id + "?alt=media" #"https://docs.google.com/spreadsheets/export?id={hoja_id}&exportFormat=xlsx"
        #1dFC94mM_taMeRhshEHBgdMRCqyxnbFa5
        url = "https://docs.google.com/spreadsheets/export?id=" + hoja_id + "&exportFormat=xlsx"
        #url = "https://docs.google.com/spreadsheets/export?id={hoja_id}&exportFormat=zip"
        res = requests.get(url, headers={"Authorization": "Bearer " + gauth.attr['credentials'].access_token,
                                         'Content-Type': 'application/vnd.google-apps.spreadsheet'})
        
        #res.content.decode()
        
        values = pd.read_excel(io.BytesIO(res.content), engine='openpyxl', header = 0)
        #print(values, '\n\n') # Lectura completa
        
        #print(len(values), '\n\n') # Total de datos

        #print(values['id_imagen'][0])
        #print(values['fecha'][0])
        #print(values['valor'][0])
        #print(values['url_imagen'][0])

    return values


# _____ PROCESAMIENTO DE LA IMAGEN _____
def extraccion_de_secciones(num_secc):
    # Sección Verde
    seccion = cv2.imread('Secciones/Area_'+num_secc+'.jpg', 0)
    _, seccion = cv2.threshold(seccion, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    secciones.append(seccion)

    borde = cv2.Canny(seccion, 50, 60)
    contornos_externos, _ = cv2.findContours(borde,  cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    borde = cv2.drawContours(seccion, contornos_externos, -1, (250, 250, 250), 6)
    _, borde = cv2.threshold(borde, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    bordes.append(borde)


def validando_objs_en_areas(img_procesada, num_secc):
    resultado = np.clip(secciones[num_secc] - img_procesada, 0, 255)
    if num_secc != 1:
        resultado = 255 - resultado
    # resultado = np.clip(resultado - bordes[num_secc], 0, 255)

    borde = cv2.Canny(secciones[num_secc], 50, 60)
    contornos_externos, _ = cv2.findContours(
        borde,  cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    resultado = cv2.drawContours(
        resultado, contornos_externos, -1, (255, 255, 255), 8)
    _, resultado_bin = cv2.threshold(
        resultado, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    _, _, stats, _ = cv2.connectedComponentsWithStats(
        resultado_bin, connectivity=4)
    ObjFil = stats[1:, :]
    # print('\nDesordenados\n\n', ObjFil)
    ObjFil = ObjFil[ObjFil[:, -1].argsort()]
    # print('\nOrdenados\n\n', ObjFil)

    # num_objetos = len(ObjFil[0])
    # print('objetos: ', num_objetos)

    '''
	if(num_objetos == 2):
		return num_secc
	else:
		return -1
	'''

    if (ObjFil[-2][4] > 1500):
        return [num_secc, ObjFil[-2]]
    else:
        return [-1, None]


def validando_area(img_procesada):
    '''
    num_secc = 0
    resultado = np.clip(secciones[num_secc] - img_procesada, 0, 255)
    resultado = 255 - resultado
    resultado = np.clip(resultado - bordes[num_secc], 0, 255)

    num_secc = 2
    resultado = np.clip(secciones[num_secc] - img_procesada, 0, 255)
    resultado = 255 - resultado
    resultado = np.clip(resultado - bordes[num_secc], 0, 255)
    '''
    band = -1
    for numero_seccion in range(0, 3, 2):
        band = validando_objs_en_areas(img_procesada, numero_seccion)
        if (band[0] != -1):
            break

    if (band[0] == -1):
        band = validando_objs_en_areas(img_procesada, 1)
        if (band[0] == -1):
            return None
        else:
            return band
    else:
        return band


def identificador_area(img, img_original):
    # Identificar y seleccionando el área del nivel [N] del manometro de la aguja
    # VERDE = 0
    # AMARILLO = 1
    # ROJO = 2

    extraccion_de_secciones('0')
    extraccion_de_secciones('1')
    extraccion_de_secciones('2')

    # Imagen original
    img_proceso = img.copy()
    img_proceso = cv2.GaussianBlur(img_proceso, (9, 9), 9)
    # img_proceso = cv2.cvtColor(img_proceso, cv2.COLOR_BGR2GRAY)

    _, img_proceso = cv2.threshold(img_proceso, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    contornos, jerarquia = cv2.findContours(img_proceso, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE)

    internal_contours = []
    for c in range(len(contornos)):
        if jerarquia[0][c][3] != -1:
            internal_contours.append(contornos[c])

    img_proceso = cv2.drawContours(img.copy(), internal_contours, -1, (255, 255, 255), 2)
    _, img_proceso = cv2.threshold(img_proceso, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    inf = validando_area(img_proceso)
    if inf is not None:
        # Mostrando resultados
        img = cv2.rectangle(img_original.copy(), (inf[-1][0], inf[-1][1]),
                            (inf[-1][0]+inf[-1][2], inf[-1][1]+inf[-1][3]),
                            (0, 255, 0), 2)

        return img, inf[0]
    else:
        return None, -1


def img_processed_manometro(id_folder):
    valor = ''

    try:
        url = buscar_img_actual(id_folder)
        imgResponse = urllib.request.urlopen(url)  # abrimos el URL
        imgNp = np.array(bytearray(imgResponse.read()), dtype=np.uint8)
        frame_original = cv2.imdecode(imgNp, -1)  # decodificamos
        imagen = frame_original.copy()#cv2.flip(frame_original, 1)


        imagen = cv2.cvtColor(imagen, cv2.COLOR_BGR2HSV)
        contrast = 1  # 0.75 .9
        brightness = -80  # 60 40
        imagen[:, :, 2] = np.clip(contrast * imagen[:, :, 2] + brightness, 0, 255)
        imagen = cv2.cvtColor(imagen, cv2.COLOR_HSV2BGR)
        imagen_original = imagen.copy()

        # Aplicar la transformada de Hough para detectar círculos
        img_processed = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
        circulos = cv2.HoughCircles(img_processed, cv2.HOUGH_GRADIENT,
                                    1, 150, param1=90, param2=80, 
                                    minRadius=0, maxRadius=0)


        if circulos is not None:
            # ___________________________ Prepocesamiento de la imagen

            # Determinando el área correspondiente al manometro
            circulos = np.round(circulos[0, :]).astype("int")
            circulos = circulos[circulos[:, -1].argsort()]
            circulos = [[ 64, 190,  60],
                         [290, 404,  63],
                         [574, 336,  81],
                         [120, 350, 118],
                         [626, 182, 119],
                         [356, 188, 148],
                         [422, 326, 187],
                         [410, 300, 170]]

            # Recorte del área de manometro
            obj_manometro = [circulos[-1][0]-circulos[-1][2], circulos[-1][1]-circulos[-1][2],
                             circulos[-1][0]+circulos[-1][2], circulos[-1][1]+circulos[-1][2]]

            # Creando una imagen de Zeros para crear la máscara del área del manometro
            area_detected = np.zeros((img_processed.shape), dtype='uint8')

            # Recortando el área del manometro en la imagen original
            area_detected = cv2.circle(area_detected, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), -1)
            img_processed_area = cv2.bitwise_and(imagen_original, imagen_original, mask=area_detected)
            img_processed_area = cv2.circle(img_processed_area, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), 1)

            img_processed_area = img_processed_area[obj_manometro[1]:obj_manometro[3],
                                                    obj_manometro[0]:obj_manometro[2]]

            # Elimando todo el fondo que no sea el área del manometro
            img_processed_area = imutils.resize(img_processed_area, width=600)
            img_processed_area = imutils.resize(img_processed_area, height=600)
            # _____________________________________________________________________________ Preprocesando a la imagen obtenida
            img_processed = img_processed_area.copy()
            imagen_original = img_processed_area.copy()
            img_processed = cv2.cvtColor(img_processed, cv2.COLOR_BGR2GRAY)
            img_processed = cv2.equalizeHist(img_processed)
            # Contorno al área del manometro
            img_processed = cv2.circle(img_processed, (300, 300), 270, (255, 255, 255), 8)

            # ___________________________ Procesamiento de la imagen
            resultado = identificador_area(img_processed, imagen_original)
            
            if resultado[0] is not None:
                if (resultado[1] == 0):
                    valor = 'La aguja indica --> nivel ALTO'
                elif (resultado[1] == 2):
                    valor = 'La aguja indica --> nivel BAJO'
                else:
                    valor = 'La aguja indica --> nivel MEDIO'

                # print(valor, resultado[1])
                valor = valor + ' [' + str(resultado[1]) + ']'
                datos_lectura[2] = str(resultado[1])
                img_processed = resultado[0]
                registro_inf2(id_folder)
            else:
                img_processed = np.zeros((600, 800, 3), np.uint8)
                cv2.putText(img_processed, 'No puede ser detectada la aguja ',
                            (30, 300), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                cv2.putText(img_processed, 'en el manometro', (30, 350),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        else:
            img_processed = np.zeros((600, 800, 3), np.uint8)
            cv2.putText(img_processed, 'No puede ser detectado el manometro',
                        (30, 300), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            cv2.putText(img_processed, 'en la imagen', (30, 350),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    except:
        img_processed = np.zeros((600, 800, 3), np.uint8)
        cv2.putText(img_processed, 'No puede ser obtenida la imagen',
                    (100, 300), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

    (_, encodedImage2) = cv2.imencode(".jpg", img_processed)
    img_processed = bytearray(encodedImage2)
    
    return valor, img_processed


@app.route("/")
def resultado_img():
    valor, img_processed = img_processed_manometro(id_folder)
    valor = str(valor)
    imagen_base64 = base64.b64encode(img_processed).decode('utf-8')
    return render_template('valor_manometro.html', imagen_base64=imagen_base64, cadena=valor)

@app.route("/lecturas_manometro")
def consulta_lecturas():
    #http://192.168.8.49:5000/lecturas_manometro
    #http://localhost:5000/lecturas_manometro
    lecturas = registro_inf(id_folder)
    print('cantidad de lecturas:',len(lecturas))
    print(lecturas)
    if len(lecturas) > 1:
        del lecturas['id_imagen'][0]
        del lecturas['fecha'][0]
        del lecturas['valor'][0]
        del lecturas['url_imagen'][0]

        lecturas2 = []
        for i in range(1, len(lecturas)):
            lecturas['fecha'][i] =  'F_'+lecturas['fecha'][i][:10]+'-H_'+lecturas['fecha'][i][11:19]
            
            lecturas2.append([lecturas['fecha'][i], lecturas['valor'][i],lecturas['url_imagen'][i]])
        
        #print('\n\n------------------\n\n',lecturas2)
        return render_template('lectura_manometro_menu.html', lecturas_mnmtr = lecturas2)#, imagen_base64=imagen_base64, cadena=valor)
    else:
        return render_template('lectura_manometro_menu2.html')
@app.route("/lectura", methods=['GET'])
def consultar_lectura():
    valor = rq.args.get('nivel')
    valor = int(valor)
    img = rq.args.get('img')
    fecha = rq.args.get('fecha')
    
    # Preparando imagen
    imgResponse = urllib.request.urlopen(img)  # abrimos el URL
    imgNp = np.array(bytearray(imgResponse.read()), dtype=np.uint8)
    imagen = cv2.imdecode(imgNp, -1)  # decodificamos

    imagen = cv2.cvtColor(imagen, cv2.COLOR_BGR2HSV)
    contrast = 1  # 0.75 .9
    brightness = -80  # 60 40
    imagen[:, :, 2] = np.clip(contrast * imagen[:, :, 2] + brightness, 0, 255)
    imagen = cv2.cvtColor(imagen, cv2.COLOR_HSV2BGR)
    #imagen_original = imagen.copy()

    circulos = [[410, 300, 170]]

    # Recorte del área de manometro
    obj_manometro = [circulos[-1][0]-circulos[-1][2], circulos[-1][1]-circulos[-1][2], circulos[-1][0]+circulos[-1][2], circulos[-1][1]+circulos[-1][2]]

    # Creando una imagen de Zeros para crear la máscara del área del manometro
    img_aux = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
    area_detected = np.zeros((img_aux.shape), dtype='uint8')

    

    # Recortando el área del manometro en la imagen original
    area_detected = cv2.circle(area_detected, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), -1)
    imagen = cv2.bitwise_and(imagen, imagen, mask=area_detected)
    imagen = cv2.circle(imagen, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), 1)



    imagen = imagen[obj_manometro[1]:obj_manometro[3],
                    obj_manometro[0]:obj_manometro[2]]

    # Elimando todo el fondo que no sea el área del manometro
    imagen = imutils.resize(imagen, width=600)
    imagen = imutils.resize(imagen, height=600)

    (_, encodedImage2) = cv2.imencode(".jpg", imagen)
    imagen = bytearray(encodedImage2)
    imagen_base64 = base64.b64encode(imagen).decode('utf-8')

    if (int(valor) == 0):
        valor = 'La aguja indica --> nivel ALTO'
    elif (int(valor) == 2):
        valor = 'La aguja indica --> nivel BAJO'
    else:
        valor = 'La aguja indica --> nivel MEDIO'

    return render_template('lectura_manometro.html', imagen_base64 = imagen_base64, cadena = valor, fech = fecha)
    
if __name__ == "__main__":
    app.run(debug = True, host = "0.0.0.0", port = "5000")