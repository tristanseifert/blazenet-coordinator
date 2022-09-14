import $ from 'cash-dom';

// bootstrap the laravel features
// require('./bootstrap');

$(document).ready(function() {
    // make logout links work
    $('a.logout').on('click', function() {
        <HTMLFormElement>document.forms['logoutform'].submit();
        return false;
    });

    // burger menu
    $(".navbar-burger").on('click', function() {
        $(".navbar-burger").toggleClass("is-active");
        $(".navbar-menu").toggleClass("is-active");
    });
});

// JS for bulma components
require('./bulma/modal');
require('./bulma/tabs');
