from flask import Flask, render_template, Response
import cv2
import urllib.request #para abrir y leer URL
import numpy as np
from matplotlib.path import Path
import math
import imutils

#Areas de los objetos
V = [75, 35, 355, 245, 57641]
A = [40, 160, 220, 180, 19566]
R = [35, 270, 190, 95, 8463]
areas = [V, A, R]
secciones = []
bordes = []



def extraccion_de_secciones(num_secc):
	# Sección Verde
	seccion = cv2.imread('Secciones/Area_'+num_secc+'.jpg', 0)
	_, seccion = cv2.threshold(seccion, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
	secciones.append(seccion)

	borde = cv2.Canny(seccion, 50, 60)
	contornos_externos, _ = cv2.findContours(borde,  cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
	borde = cv2.drawContours(seccion, contornos_externos, -1, (250,250,250), 6)
	_, borde = cv2.threshold(borde, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
	
	bordes.append(borde)



def validando_objs_en_areas(img_procesada, num_secc):
	resultado = np.clip(secciones[num_secc] - img_procesada, 0, 255)
	if num_secc != 1:
		resultado = 255 - resultado
	#resultado = np.clip(resultado - bordes[num_secc], 0, 255)

	borde = cv2.Canny(secciones[num_secc], 50, 60)
	contornos_externos, _ = cv2.findContours(borde,  cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
	resultado = cv2.drawContours(resultado, contornos_externos, -1, (255,255,255), 8)
	_, resultado_bin = cv2.threshold(resultado, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

	_, _, stats, _ = cv2.connectedComponentsWithStats(resultado_bin, connectivity=4)
	ObjFil = stats[1:, :]
	#print('\nDesordenados\n\n', ObjFil)
	ObjFil = ObjFil[ObjFil[:, -1].argsort()]
	#print('\nOrdenados\n\n', ObjFil)

	#num_objetos = len(ObjFil[0])
	#print('objetos: ', num_objetos)

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
	for numero_seccion in range(0,3,2):
		band = validando_objs_en_areas(img_procesada, numero_seccion)
		if(band[0] != -1):
			break

	if(band[0] == -1):
		band = validando_objs_en_areas(img_procesada, 1)
		if(band[0] == -1):
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
	#img_proceso = cv2.cvtColor(img_proceso, cv2.COLOR_BGR2GRAY)

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
									(0,255,0), 2)

		return img, inf[0]
	else:
		return None, -1
	


# ______________________________________________________ EJECUCIÓN DEL PRINCIPAL
for i in range(1,2):
	valor = 0
	imagen = cv2.imread('imgs_caja/ESP32CAM_20230513_16'+str(i)+'.jpg', 1)

	imagen = cv2.cvtColor(imagen, cv2.COLOR_BGR2HSV)
	contrast = 1 #0.75 .9
	brightness = -80 #60 40
	imagen[:,:,2] = np.clip(contrast * imagen[:,:,2] + brightness, 0, 255)
	imagen = cv2.cvtColor(imagen, cv2.COLOR_HSV2BGR)

	cv2.imshow('imagen',imagen)
	cv2.waitKey(0)

	imagen_original = imagen.copy()
	
	# Aplicar la transformada de Hough para detectar círculos
	img_processed = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
	circulos = cv2.HoughCircles(img_processed, cv2.HOUGH_GRADIENT,
															1, 150, param1=90, param2=80, 
															minRadius=0, maxRadius=0)
	'''
		1, 150, param1=50, param2=30, minRadius=0, maxRadius=0)
	'''
	
	#print(circulos)
	if circulos is not None:
		# ___________________________ Prepocesamiento de la imagen

		# Determinando el área correspondiente al manometro
		circulos = np.round(circulos[0, :]).astype("int")
		circulos = circulos[circulos[:, -1].argsort()]
		#print(circulos)
		#[[410 300 167]]
		circulos = [[410, 300, 170]]
		
		# Recorte del área de manometro
		obj_manometro = [circulos[-1][0]-circulos[-1][2], circulos[-1][1]-circulos[-1][2], 
											circulos[-1][0]+circulos[-1][2], circulos[-1][1]+circulos[-1][2]]

		# Creando una imagen de Zeros para crear la máscara del área del manometro
		area_detected = np.zeros((img_processed.shape), dtype = 'uint8')

		# Recortando el área del manometro en la imagen original
		area_detected = cv2.circle(area_detected, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), -1)
		img_processed_area = cv2.bitwise_and(imagen_original, imagen_original, mask = area_detected)
		img_processed_area = cv2.circle(img_processed_area, (circulos[-1][0], circulos[-1][1]), circulos[-1][2]-20, (255, 255, 255), 1)

		img_processed_area = img_processed_area[obj_manometro[1]:obj_manometro[3], obj_manometro[0]:obj_manometro[2]]
		cv2.imshow('imagen22',img_processed_area)
		cv2.waitKey(0)

		# Elimando todo el fondo que no sea el área del manometro
		img_processed_area = imutils.resize(img_processed_area, width=600)
		img_processed_area = imutils.resize(img_processed_area, height=600)

		#_____________________________________________________________________________ Preprocesando a la imagen obtenida
		img_processed = img_processed_area.copy()
		imagen_original = img_processed_area.copy()
		img_processed = cv2.cvtColor(img_processed, cv2.COLOR_BGR2GRAY)
		img_processed = cv2.equalizeHist(img_processed)
		# Contorno al área del manometro
		img_processed = cv2.circle(img_processed, (300, 300), 270, (255, 255, 255), 8)
	 
		# ___________________________ Procesamiento de la imagen
		resultado = identificador_area(img_processed, imagen_original)
		print(resultado)
		if resultado[0] is not None:
			
			if(resultado[1] == 0):
				valor = 'La aguja indica -- nivel ALTO'
			elif(resultado[1] == 2):
				valor = 'La aguja indica -- nivel BAJO'
			else:
				valor = 'La aguja indica -- nivel MEDIO'

			print(valor, resultado[1])
			cv2.imshow("Resultados", resultado[0])
			cv2.waitKey(0)
			cv2.destroyAllWindows()

cv2.destroyAllWindows()