const mix = require('laravel-mix');

/*
 |--------------------------------------------------------------------------
 | Mix Asset Management
 |--------------------------------------------------------------------------
 |
 | Mix provides a clean, fluent API for defining some Webpack build steps
 | for your Laravel applications. By default, we are compiling the CSS
 | file for the application as well as bundling up all the JS files.
 |
 */

mix.js('resources/js/app.js', 'public/js').extract()
   .sass('resources/css/app.scss', 'public/css')

// version the CSS/JS in prod
if(mix.inProduction()) {
    mix.minify().version();
}

// get rid of the horrendously annoying notifications
mix.disableNotifications();
