# Nginx HTTP Select Lang module
Chooses a suitable language from key-values and set it in the variable.

### Installation
1. Download it.
1. Compile Nginx with:
```
$ ./configure --add-module=path/to/nginx_select_lang_module
$ make install
```

#### How it works
1. Find user's selected language in the cookie name `lang`, e.g. `lang=ja`.
1. If the cookie is not defined, find it in the `Accept-Language` header.
1. Other than the avobe, use the second argument of the directive as a default language.
1. Set the language to the variable name, it must start with `$`, e.g. `$lang_selected`, the first argument of the directive.

#### Form
```
<Select-lang-directive> ::= select_lang: <language-group> { " " <language-group> }

<language-group> ::= <langauge> { ":" <language> }
```

#### Example configuration:

* `$lang_selected` is the name of a variable.
* `en`, `ja`, `zh-hans`, `zh-hant` are available languages.
* `zh-hans` is the representation of the following languages: `zh-hans`, `zh`, `zh-cn`, `zh-sg:zh-tw`.
* `zh-hant` is the representation of the following languages: `zh-hant`, and `zh-hk`.
* The first leftmost one `en` is the default language.

```
http {
  select_lang $lang_selected en ja zh-hans:zh:zh-cn:zh-sg:zh-tw zh-hant:zh-hk;
  server {
    location / {
      add_header X-Lang-Selected "$lang_selected";
      root /hoge/$lang_selected/index.html;
    }
  }
}
```
