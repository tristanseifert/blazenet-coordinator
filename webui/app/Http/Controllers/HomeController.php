<?php

namespace App\Http\Controllers;

use Illuminate\Http\Request;

/**
 * @brief Renders the home (main) page
 *
 * This page shows general status overview of the system.
 */
class HomeController extends Controller
{
    /**
     * Registers the routes.
     */
    public static function RegisterRoutes(\Illuminate\Routing\Router $router) {
        $router->get('/', [static::class, 'home'])->name('homepage');
    }

    /**
     * @brief Render the homepage.
     */
    public function home(Request $req) {
        return view('home');
    }
}
