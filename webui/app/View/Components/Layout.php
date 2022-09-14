<?php

namespace App\View\Components;

use Illuminate\View\Component;

/**
 * @brief Responsible for the overall page layout
 */
class Layout extends Component
{
    /**
     * @brief Whether the navbar is shown
     *
     * @var bool
     */
    public $showNavbar = true;

    /**
     * @brief Whether session flash messages are shown
     *
     * @var bool
     */
    public $showFlashes = true;

    /**
     * Create a new component instance.
     *
     * @param  bool $showNavbar
     * @param  bool $showFlashes
     * @return void
     */
    public function __construct($showNavbar = true, $showFlashes = true)
    {
        $this->showNavbar = $showNavbar;
        $this->showFlashes = $showFlashes;
    }

    /**
     * Get the view / contents that represent the component.
     *
     * @return \Illuminate\Contracts\View\View|\Closure|string
     */
    public function render()
    {
        return view('components.layout');
    }
}
