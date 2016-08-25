# Copyright (C) 2009 Johan Persson
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# Filename: plot.py
# Info: graph plotter
#


import sys, random
from PyQt4.QtCore import *
from PyQt4.QtGui import *


TEMP_AREA_OFFSET = 0.78
TEMP_SPACER = 19
HEATER_AREA_OFFSET = 0.05
XOFFSET = 0.055
BARWIDTH = 1


#
# Info: plotter graph
#
class plotGraph(QPixmap):
	def __init__(self, w, h, *args):
		apply(QPixmap.__init__, (self, w, h) + args)
		
		self.count = 0
		self.heaterscale = float(h) / float(800)
		
		self.tempoffset   		= int(self.height() - self.height()*TEMP_AREA_OFFSET)
		self.heateroffsetBase 	= self.height() - (self.height()*HEATER_AREA_OFFSET)
		self.xoffset 			= int(self.width() * XOFFSET)
		
		self.col = self.xoffset+1
		
		self.oventempold = 0
		self.tempsetold = 0
		self.cjtempold = 0
		
		self.fill(QColor("white"))
		self.drawGrid()

	#
	# Info: a not so hot implementation of a grid
	#
	def drawGrid(self):
		p = QPainter(self)
		p.setBackground(QColor("white"))
		h = self.height()
		w = self.width()
		
		#draw bg color
		p.fillRect(0, 0, self.xoffset , h, QBrush(QColor(255, 200, 200)))
		p.fillRect(0, h- h*0.05, w, h-h*0.05, QBrush(QColor(255, 200, 200)))
		
		# draw temperature grid
		p.drawText(10, 15, "C")
		for i in range(h - self.tempoffset, 10, -((h - self.tempoffset)/TEMP_SPACER)):
			p.setPen(QColor("lightgray"))
			p.drawLine(self.xoffset , i, w, i)
			
			p.setPen(QColor("black"))
			p.drawText(2, i+4, QString(str(h-i-(self.tempoffset))))
			p.drawLine(self.xoffset-4, i, self.xoffset , i)
		
		
		#heater grid
		#draw 0% mark
		p.setPen(QColor("black"))
		p.drawLine(self.xoffset-4,  self.heateroffsetBase  , self.xoffset ,  self.heateroffsetBase )
		p.drawText(2, self.heateroffsetBase+4, "0%")
	
		#draw 100% mark 
		p.setPen(QColor("darkgray"))
		p.drawLine(self.xoffset, (self.heateroffsetBase - h*0.12)-1, w, (self.heateroffsetBase -h*0.12)-1)
	
		p.setPen(QColor("black"))
		p.drawText(1, (self.heateroffsetBase - h*0.12)+4, "100%")
		p.drawLine(self.xoffset-4, (self.heateroffsetBase - h*0.12)-1, self.xoffset , (self.heateroffsetBase - h*0.12)-1)
		
		#draw 50% mark
		heaterycenter = self.heateroffsetBase- (self.heateroffsetBase - (self.heateroffsetBase - h*0.12))/2
		p.setPen(QColor("darkgray"))
		p.drawLine(self.xoffset, heaterycenter, w, heaterycenter)
	
		p.setPen(QColor("black"))
		p.drawText(2, heaterycenter+4, "50%")
		p.drawLine(self.xoffset-4, heaterycenter, self.xoffset, heaterycenter)
		
	
		#draw x, y lines
		p.setPen(QColor("black"))
		p.drawLine(self.xoffset, -1, self.xoffset, h- h*0.05)
		p.drawLine(self.xoffset, h- h*0.05, w+1, h-h*0.05)
		
		#draw x values
		p.drawText(10, h-3, "s")
		s = 0
		for i in range(int(self.xoffset), w, 30):
			p.drawLine(i, self.heateroffsetBase, i, self.heateroffsetBase+5)
			p.drawText(i-3, h-3, str(s))
			s = s + 30
		
		
	#
	# Info: calls update funtions for temp and heater display
	#
	def update(self, power, oventemp, tempset, cjtemp):

		self.col += BARWIDTH
	
		self.updateOvenTemp(oventemp)
		self.updateHeater(power)
		self.updateTempSet(tempset)
		self.updateCJTemp(cjtemp)
		
		
	#
	# Info: updates CJ temp on graph
	#
	def updateCJTemp(self, cjtem):
		h = self.height()
		p = QPainter(self)
		p.setBackground(QColor("white"))
		p.setRenderHint(QPainter.Antialiasing)
		
		if self.cjtempold  == 0:
			self.cjtempold = int(h- self.tempoffset) - int(cjtem)

		p.setPen(QColor("green"))
		p.drawLine(self.col-1, int(self.cjtempold) , self.col, int(h- self.tempoffset) - int(cjtem))
		
		self.cjtempold  = int(h- self.tempoffset) - int(cjtem)

		
		
	#
	# Info: updates target temp on graph
	#
	def updateTempSet(self, tempset=0):
		h = self.height()
		p = QPainter(self)
		p.setBackground(QColor("white"))
		p.setRenderHint(QPainter.Antialiasing)
		
		if self.tempsetold  == 0:
			self.tempsetold = int(h- self.tempoffset) - int(tempset)

		p.setPen(QColor("blue"))
		p.drawLine(self.col-1, int(self.tempsetold) , self.col, int(h- self.tempoffset) - int(tempset))
		
		self.tempsetold  = int(h- self.tempoffset) - int(tempset)


	#
	# Info: updates oven temp on graph
	#
	def updateOvenTemp(self, temp):
		
		h = self.height()
		p = QPainter(self)
		p.setBackground(QColor("white"))
		p.setRenderHint(QPainter.Antialiasing)
		
		if self.oventempold == 0:
			self.oventempold = (h- self.tempoffset) - temp

		p.setPen(QColor("red"))
		p.drawLine(self.col-1, self.oventempold, self.col, (h- self.tempoffset) - temp)
		
		self.oventempold = (h- self.tempoffset) - temp


	#
	# Info: updates heater usages on graph
	#
	def updateHeater(self, power):
		
		p = QPainter(self)
		p.setBackground(QColor("white"))

		p.setBrush(QColor("black"))
		
		y = float(self.heaterscale) * float(power)
		# zero division error
		if y == 0: y = 1

		minV = 255
		H = 35
		S = 255

		step = float(float(128)/float(y))
		for i in range(int(y)):
			color = QColor()
			color.setHsv(H, S, 100 + int(step * i))
			p.setPen(QPen(color))

			p.drawLine(self.col - BARWIDTH, 
						self.heateroffsetBase -i, 
						self.col, 
						self.heateroffsetBase -i)

#
# Info: Plotter Widget
#
class Plotter(QWidget):

	def __init__(self, parent):
		QWidget.__init__(self, parent)
		
		self.setGeometry(2, 2, parent.width()-4, parent.height()-4)
		self.pixmap = plotGraph(self.width(), self.height())
		self.setAutoFillBackground(True)
		

	def paintEvent(self, ev):
		p =  QPalette()
		p.setBrush(p.Background, QBrush(self.pixmap));
		self.setPalette(p)


	def update(self, heater, oventemp, tempset, cjtemp):
		self.pixmap.update(heater, oventemp, tempset, cjtemp)
		p =  QPalette()
		p.setBrush(p.Background, QBrush(self.pixmap));
		self.setPalette(p)

	
	def resetGraph(self):
		del self.pixmap
		self.pixmap = plotGraph(self.width(), self.height())
		
		#update graph
		p =  QPalette()
		p.setBrush(p.Background, QBrush(self.pixmap));
		self.setPalette(p)
		

