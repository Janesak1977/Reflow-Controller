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

# Filename: reflowmonitor.py
# Info: Main file
#

from PyQt4 import QtGui, QtCore
import ui_reflowmonitor
import sys
import time
import serial

class OvenMsg():
    """Class representing a single status message sent from the oven control hardware."""

    def __init__(self):
        """Sets sane default message contents."""
        self.state      = 'idle'
        self.time       = 0.0
        self.step       = 0
        self.target     = 0.0
        self.TtoTarget  = 0.0
        self.cmd        = 0
        self.temp       = 0.0

    def parse(self,msg):
        """Parses message contents from a comma-separated string.
        
            
        sprintf_P(tx_msg,PSTR("%s,%d,%d,%d,%d,%d,%d\\n"),    
            state_names[state],
			step,
            time,
			temp,
			TtoTarget,
            target,
			HeaterPwr)
        
        This parses that."""

        """m = msg.split(',')
        if(len(m) != 7):
            return 0

        self.state      = m.pop(0)
        self.step       = m.pop(0)
        self.time       = int(m.pop(0))*0.25
        self.temp       = int(m.pop(0))*0.25
        self.TtoTarget  = int(m.pop(0))*0.25
        self.target     = int(m.pop(0))*0.25
        self.cmd        = int(m.pop(0))/255.0"""

        self.state      = 'Idle'
        self.step       = 0
        self.time       = 0
        self.temp       = 100
        self.TtoTarget  = 240
        self.target     = 102
        self.cmd        = 255

        return 1
    

class OvenCommThread(QtCore.QThread):
    """Thread class responsible for asynchronously receiving messages from oven controller on behalf of OvenComm instance."""

    def __init__(self,parent=None):
        """Constructor; requires an OvenComm parent."""
        super(OvenCommThread,self).__init__(parent)
        self.running = True
        self.p = parent

    def __del__(self):
        """Tells thread to stop, then waits (a little while) for thread to terminate"""
        self.stop()
        self.wait()

    def run(self):
        """Triggers newMessage signal in parent OvenComm instance whenever a message is received."""
        while(self.running):
            time.sleep(2)
            msg = OvenMsg()
            msg.parse('test')         #Only for debug
            self.p.trigger_newMessage(msg)
            #if(msg.parse(self.p.s.readline()) and self.running):
             #   self.p.trigger_newMessage(msg)

    def stop(self):
        """Tells comm thread to stop running (eventually)."""
        self.running = False


class OvenComm(QtCore.QObject):
    """Class that encapsulates all serial communication with oven controller hardware."""

    newMessage = QtCore.pyqtSignal('PyQt_PyObject')     # PyQt_PyObject required for passing non-C++ types through Qt signals

    newTemp = QtCore.pyqtSignal(float)
    newTarget = QtCore.pyqtSignal(float)
    newTtoTarget = QtCore.pyqtSignal(float)
    newProfile =  QtCore.pyqtSignal(int)
    newHeater =  QtCore.pyqtSignal(int)
    newState = QtCore.pyqtSignal(str)

    def __init__(self,parent=None,port='COM1'):
        """Opens specified serial port and starts comm thread."""

        super(OvenComm,self).__init__(parent)
        
        self.thread = None
        self.s = None

        self.s = serial.Serial(port=port,timeout=0.7,baudrate=57600)

        # clear any stale data that may have been buffered prior to program start
        #time.sleep(0.5)
        #self.s.flushInput()
        #self.s.readline()

        self.thread = OvenCommThread(self)
        self.thread.start()

        self.v_cmd = 0

    def __del__(self):
        """Terminates comm thread and closes serial port."""
        self.comm.newMessage.disconnect()
        

    def trigger_newMessage(self,msg):
        """Callback for triggering Qt signals on receipt of new message by OvenCommThread."""

        self.newTemp.emit(msg.temp)
        self.newState.emit(msg.state)
        #self.newMessage.emit(msg)
        

    def go(self):
        """Callback for Go button - resets controller and starts reflow operation."""
        self.s.write("reset\ngo\n")

    def reset(self):
        """Callback for Reset button - just resets controller (stops ongoing reflow operation)."""
        self.s.write("reset\n")

    def pause(self):
        """Callback for Pause button - puts controller into pause state (profile time does not advance)."""
        self.s.write("pause\n")

    def resume(self):
        """Callback for Resume button - resumes controller after pause (profile time resumes advancing)."""
        self.s.write("resume\n")

    def manual(self,m):
        """Callback for Manual check-box - when enabled, controller's PID loop is bypassed."""
        if(m):
            self.s.write("manual: 1\n")
        else:
            self.s.write("manual: 0\n")

    def cmd(self):
        """Callback for Command slider - controls power to heating element."""
        self.v_cmd = int(t*255.0/100.0)
        print >> self.s, "cmd: %d" % (self.v_cmd)
        

    def target(self,t):
        """Callback for Target slider - controls target temperature when in Idle state."""
        t = int(t*4)
        if(t < 0):
            t = 0
        if(t > 4095):
            t = 4095
        print >> self.s, "target: %d" % (t)



class Reflow(QtGui.QMainWindow, ui_reflowmonitor.Ui_MainWindow):
  def __init__(self):

    super(Reflow, self).__init__()
    
    self.comm = OvenComm(parent=self)
    
    self.setupUi(self,self.comm)
    self.resetGui()

  def __del__(self):
      self.comm.newTemp.disconnect()
      self.comm.newState.disconnect()
      
      self.comm.thread.terminate()
      self.comm.thread.wait()
      if(self.comm.s):
        self.comm.s.close()

  def resetGui(self):
      #draw plot information lines
      pixmap = QtGui.QPixmap(QtCore.QSize(16,2))
      pixmap.fill(QtGui.QColor("red"))
      self.lblred.setPixmap(pixmap)

      pixmap.fill(QtGui.QColor("blue"))
      self.lblblue.setPixmap(pixmap)

      pixmap.fill(QtGui.QColor("green"))
      self.lblgreen.setPixmap(pixmap)


if __name__ == "__main__":
  import sys
  app = QtGui.QApplication(sys.argv)
  form = Reflow()
  form.show()
  sys.exit(app.exec_())
  