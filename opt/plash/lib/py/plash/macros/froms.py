from functools import wraps

from plash import utils
from plash.eval import hint, register_macro


def cache_container_hint(cache_key_templ):
    def decorator(func):
        @wraps(func)
        def wrapper(*args):
            cache_key = cache_key_templ.format(":".join(args)).replace("/", "%")
            container_id = utils.plash_map(cache_key)
            if not container_id:
                container_id = func(*args)
                utils.plash_map(cache_key, container_id)
            return hint("image", container_id)

        return wrapper

    return decorator


@register_macro()
@cache_container_hint("docker:{}")
def from_docker(image):
    "use image from local docker"
    utils.plash_call("import-docker", image)


@register_macro()
@cache_container_hint("lxc:{}")
def from_lxc(image):
    "use images from images.linuxcontainers.org"
    return utils.plash_call("import-lxc", image)


@register_macro()
@cache_container_hint("url:{}")
def from_url(url):
    "import image from an url"
    return utils.plash_call("import-url", url)


@register_macro()
def from_id(image):
    "specify the image from an image id"
    return hint("image", image)


class MapDoesNotExist(Exception):
    pass


@register_macro()
def from_map(map_key):
    "use resolved map as image"
    image_id = utils.plash_call("map", map_key)
    if not image_id:
        raise MapDoesNotExist("map {} not found".format(repr(map_key)))
    return hint("image", image_id)


@register_macro("from")
def from_(image):
    "guess from where to take the image"
    if image.isdigit():
        return from_id(image)
    else:
        return from_lxc(image)


@register_macro()
@cache_container_hint("github:{}")
def from_github(user_repo_pair, file="plashfile"):
    "build and use a file (default 'plashfile') from github repo"
    from urllib.request import urlopen

    url = "https://raw.githubusercontent.com/{}/master/{}".format(user_repo_pair, file)
    with utils.catch_and_die([Exception], debug=url):
        resp = urlopen(url)
    plashstr = resp.read().decode()
    return utils.plash_call("build", "--eval-stdin", input=plashstr)
