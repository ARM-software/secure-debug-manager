from com.arm.debug.dtsl.configurations import ConfigurationBaseSDF
from com.arm.debug.dtsl.configurations import DTSLv1
from com.arm.debug.dtsl.components import AHBAP
from com.arm.debug.dtsl.components import APB4AP
from com.arm.debug.dtsl.components import AXIAP
from com.arm.debug.dtsl.components import Device
from com.arm.debug.dtsl.components import DeviceInfo

coreNames_cortexM0PLUS = ["Cortex-M0+"]
coreNames_cortexA35 = ["Cortex-A35"]
coreNames_cortexM3 = ["Cortex-M3"]


# Import core specific functions
import a35_rams


class DtslScript(ConfigurationBaseSDF):
    @staticmethod
    def getOptionList():
        return [
            DTSLv1.tabSet("options", "Options", childOptions=
                [DTSLv1.tabPage("rams", "Cache RAMs", childOptions=[
                    # Turn cache debug mode on/off
                    DTSLv1.booleanOption('cacheDebug', 'Cache debug mode',
                                         description='Turning cache debug mode on enables reading the cache RAMs. Enabling it may adversely impact debug performance.',
                                         defaultValue=False, isDynamic=True),
                    DTSLv1.booleanOption('cachePreserve', 'Preserve cache contents in debug state',
                                         description='Preserve the contents of caches while the core is stopped.',
                                         defaultValue=False, isDynamic=True),
                ])]
            )
        ]
    
    def __init__(self, root):
        ConfigurationBaseSDF.__init__(self, root)
        
        self.discoverDevices()
    
    # +----------------------------+
    # | Target dependent functions |
    # +----------------------------+
    
    def discoverDevices(self):
        '''Find and create devices'''
        
        # MEMAP devices
        AHBAP(self, self.findDevice("CSMEMAP_0"), "CSMEMAP_0")
        APB4AP(self, self.findDevice("CSMEMAP_1"), "CSMEMAP_1")
        AXIAP(self, self.findDevice("CSMEMAP_2"), "CSMEMAP_2")
        AHBAP(self, self.findDevice("CSMEMAP_3"), "CSMEMAP_3")
        
        self.cortexM0PLUScores = []
        for coreName in (coreNames_cortexM0PLUS):
            # Create core
            coreDevice = Device(self, self.findDevice(coreName), coreName)
            deviceInfo = DeviceInfo("core", "Cortex-M0+")
            coreDevice.setDeviceInfo(deviceInfo)
            self.cortexM0PLUScores.append(coreDevice)
            self.addDeviceInterface(coreDevice)
            
        self.cortexM3cores = []
        for coreName in (coreNames_cortexM3):
            # Create core
            coreDevice = Device(self, self.findDevice(coreName), coreName)
            deviceInfo = DeviceInfo("core", "Cortex-M3")
            coreDevice.setDeviceInfo(deviceInfo)
            self.cortexM3cores.append(coreDevice)
            self.addDeviceInterface(coreDevice)
            
        self.cortexA35cores = []
        for coreName in (coreNames_cortexA35):
            # Create core
            coreDevice = a35_rams.A35CoreDevice(self, self.findDevice(coreName), coreName)
            deviceInfo = DeviceInfo("core", "Cortex-A35")
            coreDevice.setDeviceInfo(deviceInfo)
            self.cortexA35cores.append(coreDevice)
            self.addDeviceInterface(coreDevice)
            a35_rams.registerInternalRAMs(coreDevice)
            
    # +--------------------------------+
    # | Callback functions for options |
    # +--------------------------------+
    
    def optionValuesChanged(self):
        '''Callback to update the configuration state after options are changed'''
        if not self.isConnected():
            self.setInitialOptions()
        
        self.updateDynamicOptions()
        
    def setInitialOptions(self):
        '''Set the initial options'''
        
    def updateDynamicOptions(self):
        '''Update the dynamic options'''
        
        for core in range(len(self.cortexA35cores)):
            a35_rams.applyCacheDebug(configuration = self,
                                     optionName = "options.rams.cacheDebug",
                                     device = self.cortexA35cores[core])
            a35_rams.applyCachePreservation(configuration = self,
                                            optionName = "options.rams.cachePreserve",
                                            device = self.cortexA35cores[core])
        
class DtslScript_DSTREAM_ST(DtslScript):
    @staticmethod
    def getOptionList():
        return [
            DTSLv1.tabSet("options", "Options", childOptions=
                [DTSLv1.tabPage("rams", "Cache RAMs", childOptions=[
                    # Turn cache debug mode on/off
                    DTSLv1.booleanOption('cacheDebug', 'Cache debug mode',
                                         description='Turning cache debug mode on enables reading the cache RAMs. Enabling it may adversely impact debug performance.',
                                         defaultValue=False, isDynamic=True),
                    DTSLv1.booleanOption('cachePreserve', 'Preserve cache contents in debug state',
                                         description='Preserve the contents of caches while the core is stopped.',
                                         defaultValue=False, isDynamic=True),
                ])]
            )
        ]

class DtslScript_DSTREAM_PT(DtslScript):
    @staticmethod
    def getOptionList():
        return [
            DTSLv1.tabSet("options", "Options", childOptions=
                [DTSLv1.tabPage("rams", "Cache RAMs", childOptions=[
                    # Turn cache debug mode on/off
                    DTSLv1.booleanOption('cacheDebug', 'Cache debug mode',
                                         description='Turning cache debug mode on enables reading the cache RAMs. Enabling it may adversely impact debug performance.',
                                         defaultValue=False, isDynamic=True),
                    DTSLv1.booleanOption('cachePreserve', 'Preserve cache contents in debug state',
                                         description='Preserve the contents of caches while the core is stopped.',
                                         defaultValue=False, isDynamic=True),
                ])]
            )
        ]

class DtslScript_DebugAndOnChipTrace(DtslScript):
    @staticmethod
    def getOptionList():
        return [
            DTSLv1.tabSet("options", "Options", childOptions=
                [DTSLv1.tabPage("rams", "Cache RAMs", childOptions=[
                    # Turn cache debug mode on/off
                    DTSLv1.booleanOption('cacheDebug', 'Cache debug mode',
                                         description='Turning cache debug mode on enables reading the cache RAMs. Enabling it may adversely impact debug performance.',
                                         defaultValue=False, isDynamic=True),
                    DTSLv1.booleanOption('cachePreserve', 'Preserve cache contents in debug state',
                                         description='Preserve the contents of caches while the core is stopped.',
                                         defaultValue=False, isDynamic=True),
                ])]
            )
        ]

