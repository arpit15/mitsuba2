import os
import numpy as np
import mitsuba

# Set the desired mitsuba variant
mitsuba.set_variant('packet_spectral')

from mitsuba.core import Bitmap, Struct, Thread
from mitsuba.core.xml import load_file

# Absolute or relative path to the XML file
# filename = 'path/to/my/scene.xml'
filename = '/Users/virtualworld/projects/sensor_optim/gelsight_model/gelsight_core_model_mitsuba2.xml'

# Add the scene directory to the FileResolver's search path
Thread.thread().file_resolver().append(os.path.dirname(filename))

# Load the actual scene
scene = load_file(filename)

# Call the scene's integrator to render the loaded scene
scene.integrator().render(scene, scene.sensors()[0])

# After rendering, the rendered data is stored in the film
film = scene.sensors()[0].film()

# Write out rendering as high dynamic range OpenEXR file
film.set_destination_file('/tmp/output.exr')
film.develop()

# Write out a tonemapped JPG of the same rendering
# bmp = film.bitmap(raw=True)
# bmp.convert(Bitmap.PixelFormat.RGB, Struct.Type.UInt8, srgb_gamma=True).write('/tmp/output.jpg')

# Get linear pixel values as a numpy array for further processing
# bmp_linear_rgb = bmp.convert(Bitmap.PixelFormat.RGB, Struct.Type.Float32, srgb_gamma=False)
# image_np = np.array(bmp_linear_rgb)
# print(image_np.shape)
