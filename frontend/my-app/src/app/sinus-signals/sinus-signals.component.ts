import { Component, OnInit, AfterViewInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { Chart, registerables } from 'chart.js';

@Component({
  selector: 'app-sinus-signals',
  standalone: true,
  imports: [CommonModule, FormsModule],
  templateUrl: './sinus-signals.component.html',
  styleUrls: ['./sinus-signals.component.css']
})
export class SinusSignalsComponent implements OnInit, AfterViewInit {
  title = 'Sinus Signals';
  amplitude: number = 1;
  frequency: number = 0.5;
  noiseLevel: number = 0;
  status: string = '';
  phase: number = 0;
  chart: Chart | undefined;

  constructor() {
    Chart.register(...registerables);
  }

  ngOnInit() {
    console.log('SinusSignalsComponent: Initializing...');
  }

  ngAfterViewInit() {
    this.generateSignal();
  }

  generateSignal() {
    console.log('SinusSignalsComponent: Generating signal');
    const canvas = document.getElementById('signalChart') as HTMLCanvasElement;
    if (!canvas) {
      this.status = 'Error: Chart canvas not found';
      return;
    }

    const ctx = canvas.getContext('2d');
    if (!ctx) {
      this.status = 'Error: Canvas context not available';
      return;
    }

    const dataPoints = 100;
    const x = Array.from({ length: dataPoints }, (_, i) => i / 10);
    const y = x.map((t) => 
      this.amplitude * Math.sin(2 * Math.PI * this.frequency * t + this.phase) +
      (Math.random() - 0.5) * this.noiseLevel
    );

    if (this.chart) {
      this.chart.destroy();
    }

    this.chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: x.map(t => t.toFixed(1)),
        datasets: [{
          label: 'Sinus Signal',
          data: y,
          borderColor: 'blue',
          fill: false
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: {
            type: 'category',
            title: { display: true, text: 'Time (s)' }
          },
          y: {
            title: { display: true, text: 'Amplitude' }
          }
        }
      }
    });

    this.status = 'Signal generated';
    setTimeout(() => {
      this.status = '';
    }, 20000);
  }

  createScenarioFile() {
    console.log('SinusSignalsComponent: Creating scenario file');
    this.status = 'Scenario file created (placeholder)';
    setTimeout(() => {
      this.status = '';
    }, 20000);
  }
}