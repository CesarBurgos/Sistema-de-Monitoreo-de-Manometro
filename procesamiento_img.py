from flask import Flask, render_template, Response
import cv2
import urllib.request #para abrir y leer URL
import numpy as np
from matplotlib.path import Path
import math
import imutils

valor_min_manometro = 0
valor_max_manometro = 100

def solve(bl, tr, p):
    if (p[0] > bl[0] and p[0] < tr[0] and p[1] > bl[1] and p[1] < tr[1]) :
      return True
    else:
      return False

def identificador_aguja(img, arg):
	_, output, stats, _ = cv2.connectedComponentsWithStats(img, connectivity=4)
	ObjFil = stats[1:, :]
	sizes = stats[1:, -1]
	ObjFil2 = ObjFil.copy()
	ObjFil2 = ObjFil2[ObjFil2[:, -1].argsort()]
	sizes2 = ObjFil2[1:, -1]

	i = 0
	imgObj = np.zeros((img.shape), dtype = 'uint8')
	for j in range(0, len(sizes)):
		#AGUJA
		if sizes[j] == sizes2[-2]: #sizes2[-3]: # 500
			imgObj[output == j + 1] = 255
			break
		i+=1
	
    # Find Canny edges
	canny = cv2.Canny(imgObj, 50, 55, apertureSize = 3)

	return ObjFil2[-2]

def identificador_area(img_bin_inv, output, stats, cant_px):
	sizes = stats[1:, -1]
	imgObj = np.zeros((img_bin_inv.shape), dtype = 'uint8')
	
	for j in range(0, len(sizes)):
		if sizes[j] == cant_px:
			imgObj[output == j + 1] = 255
			break

	kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
	imgObj = cv2.dilate(imgObj, kernel, iterations = 1)

	_, output, stats, _ = cv2.connectedComponentsWithStats(imgObj, connectivity=4)
	ObjFil = stats[1:, :]

	cv2.imwrite('Area_'+str(cant_px)+'.jpg', imgObj)

	#print('\n\n\n',output,'\n\n\n')

	cv2.imshow("Area_"+str(cant_px), imgObj)
	cv2.waitKey(0)
	cv2.destroyAllWindows()

	return imgObj


for i in range(1,2):
	valor = 0
	#imagen = cv2.imread('imgs/cam-hi_'+str(i)+'.jpg', 1)
	imagen = cv2.imread('imgs_caja/ESP32CAM_20230513_16'+str(i)+'.jpg', 1)

	# Mostrar la imagen con los círculos detectados
	cv2.imshow("Original", imagen)
	cv2.waitKey(0)
	cv2.destroyAllWindows()

	imagen = cv2.cvtColor(imagen, cv2.COLOR_BGR2HSV)
	contrast = 1 #0.75 .9
	brightness = -80 #60 40
	imagen[:,:,2] = np.clip(contrast * imagen[:,:,2] + brightness, 0, 255)
	imagen = cv2.cvtColor(imagen, cv2.COLOR_HSV2BGR)
	imagen_original = imagen.copy()
	
	# Mostrar la imagen con los círculos detectados
	cv2.imshow("Modificando_imagen", imagen)
	cv2.waitKey(0)
	cv2.destroyAllWindows()

	img_processed = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
	# Aplicar la transformada de Hough para detectar círculos
	circulos = cv2.HoughCircles(img_processed, 
					cv2.HOUGH_GRADIENT, 1, 150, param1=90, param2=80, minRadius=0, maxRadius=0)
					#param1=50, param2=30, minRadius=0, maxRadius=0)
	

	if circulos is not None:
		# ___________________________ Prepocesamiento de la imagen

		# Determinando el área correspondiente al manometro
		circulos = np.round(circulos[0, :]).astype("int")
		circulos = circulos[circulos[:, -1].argsort()]
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

		img_processed_area = img_processed_area[obj_manometro[1]:obj_manometro[3],
																						obj_manometro[0]:obj_manometro[2]]

		# Mostrar la imagen del área del manometro
		cv2.imshow("Area_manometro", img_processed_area)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		# Elimando todo el fondo que no sea el área del manometro
		img_processed_area = imutils.resize(img_processed_area, width=600)
		img_processed_area = imutils.resize(img_processed_area, height=600)

		
		#_____________________________________________________________________________ Preprocesando a la imagen obtenida
		#Filtro gaussiano
		#img_processed_area = cv2.GaussianBlur(img_processed_area, (5, 5), 0)
		img_processed = img_processed_area.copy()
		cv2.imshow("Área de mediciones Sin ecualizar", img_processed)
		cv2.waitKey(0)
		cv2.destroyAllWindows()
		
		img_processed = cv2.cvtColor(img_processed, cv2.COLOR_BGR2GRAY)
		img_processed_area = cv2.GaussianBlur(img_processed_area, (5, 5), 0)
		img_processed = cv2.equalizeHist(img_processed)
		cv2.imshow("Área de mediciones - ecualizada", img_processed)
		cv2.waitKey(0)
		cv2.destroyAllWindows()
	 
		# ___________________________ Procesamiento de la imagen
		# Detectando bordes dentro de la imagen
		canny = cv2.Canny(img_processed, 50, 100) #cv2.Canny(img_processed, 50, 80) #50, 55 #70, 80

		cv2.imshow("Canny", canny)
		cv2.waitKey(0)
		cv2.destroyAllWindows()
 
		# Encuentra los contornos de la imagen
		contornos, _ = cv2.findContours(canny,  cv2.RETR_LIST, cv2.CHAIN_APPROX_SIMPLE)
		img_processed = cv2.drawContours(img_processed_area, contornos, -1, (0, 255, 0), 2)

		# Mostrar la imagen con los círculos detectados
		cv2.imshow("contornos dibujados", img_processed)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		img_processed = cv2.cvtColor(img_processed, cv2.COLOR_BGR2GRAY)
		_, img_bin_inv = cv2.threshold(img_processed, 10, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
		
		_, output, stats, _ = cv2.connectedComponentsWithStats(img_bin_inv, connectivity=4)
		ObjFil = stats[1:, :]
		sizes = stats[1:, -1]

		print(ObjFil)

		img_bin_inv = cv2.circle(img_bin_inv, (300, 300), 265, (0, 0, 0), 4)

		
	 
		kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
		img_bin_inv = cv2.dilate(img_bin_inv, kernel, iterations = 3)
		# Contorno al área del manometro
		img_bin_inv = cv2.circle(img_bin_inv, (300, 300), 265, (0, 0, 0), 4)

		cv2.imshow("Binarizada", img_bin_inv)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		#print(ObjFil)
		n_obj = 13 
		# 4 --- Área color Rojo - 7
		# 1 --- Área color Verde
		# 5 --- Punta de aguja

		# AYUDA PARA SABER QUE OBJETO USO
		img_bin_inv = cv2.rectangle(img_bin_inv, (ObjFil[n_obj][0], ObjFil[n_obj][1]), 
																(ObjFil[n_obj][0] + ObjFil[n_obj][2], ObjFil[n_obj][1] + ObjFil[n_obj][3]), 
															  (255,255,255), 3)

		Area_circulo = [int(ObjFil[n_obj][0] + (ObjFil[n_obj][2]/2))+36,#34,
										int(ObjFil[n_obj][1] + (ObjFil[n_obj][3]/2))]#12]


		# Límite en área 	Roja 1
		img_bin_inv = cv2.line(img_bin_inv, (40, 305), 
													(Area_circulo[0] - 180, Area_circulo[1]-30), 
													(255,255,255), 3)
		
		# Límite en área 	Roja 2
		img_bin_inv = cv2.line(img_bin_inv, (50, 365), 
													(Area_circulo[0] - 180, Area_circulo[1]), 
													(255,255,255), 3)

		# Límite en área verde 2 
		img_bin_inv = cv2.line(img_bin_inv, (420, 70), (400, 245), (255,255,255), 3)

		# Recorte del área de la aguja
		img_bin_inv = cv2.circle(img_bin_inv, 
			(Area_circulo[0], Area_circulo[1]+10), 200, (0, 0, 0), -1)#3

		# Límite en área 	Roja 3
		img_bin_inv = cv2.line(img_bin_inv, (Area_circulo[0] - 195, Area_circulo[1]-35), 
																				(Area_circulo[0] - 200, Area_circulo[1]-4), 
																				(255,255,255), 3) #W

		_, output, stats, _ = cv2.connectedComponentsWithStats(img_bin_inv, connectivity=4)
		ObjFil = stats[1:, :] 

		print(ObjFil)

		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Identificador de secciones", img_bin_inv)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		img1 = identificador_area(img_bin_inv, output, stats, 59110)

		img2 = identificador_area(img_bin_inv, output, stats, 7525)

		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Imagen -- Binarizada invertida", img_bin_inv)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		img_bin = 255 - img_bin_inv
		img_bin = cv2.circle(img_bin, (300, 300), 265, (0, 0, 0), 4)
		# Recorte del área de la aguja
		img_bin = cv2.circle(img_bin, (Area_circulo[0], Area_circulo[1]+10), 200, (0, 0, 0), 3)
		
		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Imagen -- Binarizada xxxx", img_bin)
		cv2.waitKey(0)
		cv2.destroyAllWindows()


		_, output, stats, _ = cv2.connectedComponentsWithStats(img_bin, connectivity=4)
		ObjFil = stats[1:, :]
		sizes = stats[1:, -1]
		print(ObjFil)
		
		img3 = identificador_area(img_bin, output, stats, 20960)

		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Imagen -- Binarizada ", img_bin)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		img_r = img1.copy()
		img_r = cv2.bitwise_xor(img_r,img2)
		img_r = cv2.bitwise_xor(img_r,img3)
		cv2.imwrite('AreaTotal.jpg', img_r)

		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Imagen -- Binarizada ", img_r)
		cv2.waitKey(0)
		cv2.destroyAllWindows()

		
		'''_, img_bin = cv2.threshold(img_processed, 10, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
	 
		# Mostrar la imagen con los círculos detectados
		cv2.imshow("Imagen -- Binarizada", img_bin)
		cv2.waitKey(0)
		cv2.destroyAllWindows()'''

	'''	 
		#img_processed = cv2.morphologyEx(img_processed, cv2.MORPH_OPEN, kernel, iterations = 3)
		img_processed = cv2.morphologyEx(img_processed, cv2.MORPH_CLOSE, kernel, iterations = 3)
		
		procesa_umbral = img_processed.copy()
	'''
		
	cv2.destroyAllWindows()