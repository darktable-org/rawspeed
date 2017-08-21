================================================================================
Camera Support
================================================================================

.. exec::
    from xml.dom.minidom import parse
    import xml.dom.minidom

    DOMTree = xml.dom.minidom.parse("data/cameras.xml")
    cameras = DOMTree.documentElement.getElementsByTagName("Camera")

    unique_makes = dict()

    for camera in cameras:
        make = ''
        model = ''

        if len(camera.getElementsByTagName('ID')) > 0:
            ID = camera.getElementsByTagName('ID')[0]
            make = ID.getAttribute("make")
            model = ID.getAttribute("model")
        else:
            make = camera.getAttribute("make")
            model = camera.getAttribute("model")

        unique_makes[make] = unique_makes.get(make, dict())
        unique_makes[make][model] = unique_makes[make].get(model, dict())
        unique_makes[make][model]['modes'] = unique_makes[
            make][model].get('modes', set())
        unique_makes[make][model]['aliases'] = unique_makes[
            make][model].get('aliases', set())

        mode = camera.getAttribute("mode")
        if mode == '':
            mode = 'Default mode'
        if camera.hasAttribute("supported") and camera.getAttribute("supported") == 'no':
            mode += " - *unsupported*"

        unique_makes[make][model]['modes'].add(mode)

        for alias in camera.getElementsByTagName('Alias'):
            if alias.getAttribute("id") != '':
                unique_makes[make][model][
                    'aliases'].add(alias.getAttribute("id"))
            else:
                unique_makes[make][model][
                    'aliases'].add(alias.childNodes[0].data)

    from collections import OrderedDict

    unique_makes = OrderedDict(sorted(unique_makes.items()))

    print("There are %i known camera makers, " % len(unique_makes))

    count_unique_makesmodels = 0
    for make, models in unique_makes.items():
        count_unique_makesmodels += len(models)

    print("%i known unique camera models.\n" % count_unique_makesmodels)

    print(
        "Any support is impossible without the samples.\nCurrently, |rpu-button-cameras| cameras have samples, with total count of |rpu-button-samples| unique samples. **Please contribute samples**!\n\n")

    for make, models in unique_makes.items():
        print(make)
        print('=' * len(make), "\n")

        print("Known cameras: ", len(models), "\n")

        models = OrderedDict(sorted(models.items()))
        for model, content in models.items():
            if len(content['modes']) > 1 or 'Default mode' in content['modes']:
                print("* ", model, "\n")
            else:
                print("* ", model, " - *unsupported*\n")

            if len(content['modes']) > 1:
                print("  modes:\n")
                for mode in content['modes']:
                    print("  * ", mode, "\n")

            if len(content['aliases']) > 1:
                print("  aliases:\n")
                for alias in content['aliases']:
                    print("  * ", alias, "\n")

        print("\n\n")


.. |rpu-button-cameras| image:: https://raw.pixls.us/button-cameras.svg
    :target: https://raw.pixls.us/

.. |rpu-button-samples| image:: https://raw.pixls.us/button-samples.svg
    :target: https://raw.pixls.us/
