import importlib
import logging
import pkgutil
from litsupport.modules import modules

# Load our custom modules
for importer, modname, ispkg in pkgutil.walk_packages(path=__path__,
                                                      prefix=__name__ + '.'):
    module = importlib.import_module(modname)
    if not hasattr(module, 'mutatePlan'):
        logging.error('Skipping %s: No mutatePlan function' % modname)
        continue
    assert modname.startswith('litsupport_rawspeed.modules.')
    shortname = modname[len('litsupport_rawspeed.modules.'):]
    modules[shortname] = module
    logging.info("Loaded test module %s" % module.__file__)
