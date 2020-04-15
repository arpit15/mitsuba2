import mitsuba 

mitsuba.set_variant("scalar_spectral")

from mitsuba.render import Texture
from mitsuba.core.xml import load_string
import inspect
from ipdb import set_trace

spec_from_xml = load_string('''<spectrum version='2.0.0' type='d65' name="intensity">
        </spectrum>''')

spec = Texture.D65(1)

set_trace()